#include "src/common/replay/cli.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace replay {
namespace {

std::string readFile(const char *path) {
    std::FILE *handle = std::fopen(path, "rb");
    if (handle == nullptr) return std::string();
    std::string out;
    char buffer[4096];
    size_t got;
    while ((got = std::fread(buffer, 1, sizeof(buffer), handle)) > 0) {
        out.append(buffer, got);
    }
    std::fclose(handle);
    return out;
}

std::vector<std::string> splitLines(const std::string &text) {
    std::vector<std::string> out;
    std::string line;
    for (char c : text) {
        if (c == '\n') {
            out.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

}

int runCli(Probe &probe, const Script &script, const char *defaultSaveDir,
           int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <resource-dir> --check|--write <baseline.tsv>\n",
                     argv[0]);
        return 2;
    }
    const char *resourceDir = argv[1];
    bool write = std::strcmp(argv[2], "--write") == 0;
    const char *baselinePath = argv[3];

    std::string saveDir = defaultSaveDir;
    if (const char *tmp = std::getenv("TEST_TMPDIR")) {
        saveDir = std::string(tmp) + "/rms-replay";
    }

    bool bootOk = false;
    std::vector<TickResult> rows = run(probe, script, resourceDir, saveDir, &bootOk);
    if (!bootOk) {
        std::fprintf(stderr, "REPLAY FAIL: boot did not complete\n");
        return 1;
    }

    std::string actual = toTsv(script.name, rows);

    if (write) {
        std::FILE *handle = std::fopen(baselinePath, "wb");
        if (handle == nullptr) {
            std::fprintf(stderr, "cannot write %s\n", baselinePath);
            return 1;
        }
        std::fwrite(actual.data(), 1, actual.size(), handle);
        std::fclose(handle);
        std::printf("wrote %s (%d ticks)\n", baselinePath, (int)rows.size());
        return 0;
    }

    std::string expected = readFile(baselinePath);
    if (expected.empty()) {
        std::fprintf(stderr, "REPLAY FAIL: no baseline at %s\n", baselinePath);
        return 1;
    }
    if (expected == actual) {
        std::printf("REPLAY OK: %d ticks match %s\n", (int)rows.size(), baselinePath);
        return 0;
    }

    std::vector<std::string> want = splitLines(expected);
    std::vector<std::string> got = splitLines(actual);
    for (size_t n1 = 0; n1 < want.size() || n1 < got.size(); ++n1) {
        std::string a = n1 < want.size() ? want[n1] : std::string();
        std::string b = n1 < got.size() ? got[n1] : std::string();
        if (a != b) {
            std::fprintf(stderr, "REPLAY FAIL: first divergence at line %d\n", (int)n1 + 1);
            std::fprintf(stderr, "  baseline: %s\n", a.c_str());
            std::fprintf(stderr, "  actual:   %s\n", b.c_str());
            break;
        }
    }
    return 1;
}

}
