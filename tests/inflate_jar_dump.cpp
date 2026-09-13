// Reads the real game JARs with the vendored inflater and checks every entry
// against a Bazel-generated report from Python's zipfile.
//
// `inflate_unit_test` pins the decoder against known DEFLATE vectors. This asks
// the other question: is it right on the actual archives the port exists to
// read? A decoder can be correct on a handful of hand-picked streams and wrong
// on a 170 KB JAR that exercises dynamic Huffman tables, long back-references
// and every block type in one file.
//
// Bazel generates the expected values with Python's `zipfile` (zlib underneath)
// entry by entry for the two canonical game archives. The answer therefore
// comes from an independent implementation, not this decoder's own output.
//
// One line per entry: archive, name, uncompressed size, and a digest of the
// bytes.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/common/platform/inflate.hpp"

namespace {

// FNV-1a, 64-bit. Any stable checksum would do; it needs no table and no
// dependency, and the comparison is against another implementation of the same
// function rather than against a standard.
uint64_t digest(const std::vector<uint8_t> &bytes) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < bytes.size(); ++i) {
        h ^= (uint64_t)bytes[i];
        h *= 1099511628211ull;
    }
    return h;
}

bool slurp(const char *path, std::vector<uint8_t> *out) {
    std::FILE *f = std::fopen(path, "rb");
    if (f == nullptr) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out->resize((size_t)(n < 0 ? 0 : n));
    if (!out->empty()) {
        size_t got = std::fread(out->data(), 1, out->size(), f);
        out->resize(got);
    }
    std::fclose(f);
    return true;
}

bool dumpJar(const char *path, std::vector<std::string> *lines) {
    std::vector<uint8_t> bytes;
    if (!slurp(path, &bytes)) {
        std::fprintf(stderr, "cannot read %s\n", path);
        return false;
    }

    std::vector<platform::ZipEntry> entries;
    std::string error;
    if (!platform::readZip(bytes, &entries, &error)) {
        std::fprintf(stderr, "%s: %s\n", path, error.c_str());
        return false;
    }

    // The archive's own basename, so one sentinel covers several JARs and a
    // mismatch says which. Bazel hands us a full path that varies by machine.
    std::string base = path;
    size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);

    for (size_t i = 0; i < entries.size(); ++i) {
        char buf[512];
        std::snprintf(buf, sizeof(buf), "%s\t%s\t%u\t%016llx", base.c_str(),
                      entries[i].name.c_str(), (unsigned)entries[i].data.size(),
                      (unsigned long long)digest(entries[i].data));
        lines->push_back(std::string(buf));
    }
    return true;
}

std::vector<std::string> readLines(const std::vector<uint8_t> &bytes) {
    std::vector<std::string> out;
    std::string current;
    for (size_t i = 0; i < bytes.size(); ++i) {
        char c = (char)bytes[i];
        if (c == '\n') {
            if (!current.empty() && current[current.size() - 1] == '\r') {
                current.resize(current.size() - 1);
            }
            if (!current.empty()) out.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: %s <archive.jar> [...] [--check <sentinel.tsv>]\n",
                     argv[0]);
        return 2;
    }

    const char *check = nullptr;
    std::vector<const char *> jars;
    for (int n = 1; n < argc; ++n) {
        if (std::strcmp(argv[n], "--check") == 0 && n + 1 < argc) {
            check = argv[++n];
        } else {
            jars.push_back(argv[n]);
        }
    }

    std::vector<std::string> lines;
    for (size_t i = 0; i < jars.size(); ++i) {
        if (!dumpJar(jars[i], &lines)) return 1;
    }
    std::sort(lines.begin(), lines.end());

    if (check == nullptr) {
        for (size_t i = 0; i < lines.size(); ++i) {
            std::printf("%s\n", lines[i].c_str());
        }
        return 0;
    }

    std::vector<uint8_t> want;
    if (!slurp(check, &want)) {
        std::fprintf(stderr, "cannot read sentinel %s\n", check);
        return 1;
    }
    std::vector<std::string> expected = readLines(want);

    if (expected == lines) {
        std::printf("inflate: %u entries match the sentinel\n", (unsigned)lines.size());
        return 0;
    }

    std::fprintf(stderr, "inflate: output differs from %s\n", check);
    size_t n = expected.size() > lines.size() ? expected.size() : lines.size();
    size_t shown = 0;
    for (size_t i = 0; i < n && shown < 10; ++i) {
        std::string a = i < expected.size() ? expected[i] : std::string("<missing>");
        std::string b = i < lines.size() ? lines[i] : std::string("<missing>");
        if (a != b) {
            std::fprintf(stderr, "  expected: %s\n  actual:   %s\n", a.c_str(),
                         b.c_str());
            ++shown;
        }
    }
    return 1;
}
