#include "src/common/render/frame_check.hpp"

#include <cstdio>
#include <vector>

namespace framecheck {
namespace {

const int32_t kBlocks = 16;

std::vector<double> blockAverages(const render::Surface &surface) {
    std::vector<double> out((size_t)kBlocks * kBlocks, 0.0);
    if (surface.empty()) {
        return out;
    }
    for (int32_t by = 0; by < kBlocks; ++by) {
        for (int32_t bx = 0; bx < kBlocks; ++bx) {
            int32_t x0 = bx * surface.width / kBlocks;
            int32_t x1 = (bx + 1) * surface.width / kBlocks;
            int32_t y0 = by * surface.height / kBlocks;
            int32_t y1 = (by + 1) * surface.height / kBlocks;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            double sum = 0.0;
            int32_t count = 0;
            for (int32_t y = y0; y < y1 && y < surface.height; ++y) {
                const uint32_t *row = surface.row(y);
                for (int32_t x = x0; x < x1 && x < surface.width; ++x) {
                    uint32_t px = row[x];
                    double r = (double)((px >> 16) & 0xFF);
                    double g = (double)((px >> 8) & 0xFF);
                    double b = (double)(px & 0xFF);
                    sum += 0.299 * r + 0.587 * g + 0.114 * b;
                    ++count;
                }
            }
            out[(size_t)by * kBlocks + bx] = count > 0 ? sum / (double)count : 0.0;
        }
    }
    return out;
}

double structureSimilarity(const render::Surface &a, const render::Surface &b) {
    std::vector<double> ba = blockAverages(a);
    std::vector<double> bb = blockAverages(b);
    double total = 0.0;
    for (size_t n = 0; n < ba.size(); ++n) {
        double d = ba[n] - bb[n];
        total += d < 0 ? -d : d;
    }
    double mean = total / (double)ba.size();
    double pct = 100.0 * (1.0 - mean / 255.0);
    return pct < 0.0 ? 0.0 : pct;
}

render::Surface diffImage(const render::Surface &actual, const render::Surface &expected) {
    render::Surface out(actual.width, actual.height);
    for (int32_t y = 0; y < actual.height; ++y) {
        const uint32_t *ar = actual.row(y);
        const uint32_t *er = expected.row(y);
        uint32_t *orow = out.row(y);
        for (int32_t x = 0; x < actual.width; ++x) {
            uint32_t av = ar[x];
            if (av != er[x]) {
                orow[x] = 0xFFFF0000u;
                continue;
            }
            uint32_t r = ((av >> 16) & 0xFF) / 4;
            uint32_t g = ((av >> 8) & 0xFF) / 4;
            uint32_t b = (av & 0xFF) / 4;
            orow[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
    return out;
}

}

Result compare(const render::Surface &frame, const std::string &baselineDir,
               const std::string &name, const std::string &failDir) {
    Result result;
    if (baselineDir.empty()) {
        result.note = "no baseline dir";
        return result;
    }
    render::Surface expected;
    std::string path = baselineDir + "/" + name + ".png";
    if (!render::readPng(path, &expected)) {
        result.note = "no baseline " + path;
        return result;
    }
    result.compared = true;
    if (expected.width != frame.width || expected.height != frame.height) {
        result.sizeMismatch = true;
        result.note = "expected " + std::to_string(expected.width) + "x" +
                      std::to_string(expected.height) + ", got " +
                      std::to_string(frame.width) + "x" + std::to_string(frame.height);
        return result;
    }

    size_t changed = 0;
    for (size_t n = 0; n < frame.pixels.size(); ++n) {
        if (frame.pixels[n] != expected.pixels[n]) ++changed;
    }
    result.match = changed == 0;
    result.changedPct = 100.0 * (double)changed / (double)frame.pixels.size();
    if (result.match) {
        return result;
    }
    result.structurePct = structureSimilarity(frame, expected);
    if (!failDir.empty()) {
        render::writePng(frame, failDir + "/" + name + ".actual.png");
        render::writePng(diffImage(frame, expected), failDir + "/" + name + ".diff.png");
    }
    return result;
}

std::string describe(const Result &result) {
    if (!result.compared) {
        return "no baseline";
    }
    if (result.sizeMismatch) {
        return "SIZE CHANGED (" + result.note + ")";
    }
    if (result.match) {
        return "matches baseline";
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "DIFFERS  %.2f%% pixels, %.1f%% structure",
                  result.changedPct, result.structurePct);
    return std::string(buf);
}

}
