// DEFLATE and the zip reader, against streams whose right answers are known.
//
// This is the one piece of the port that is a format implementation rather than
// a transliteration, so it gets tested the way a format implementation should
// be: not "does it run" but "does it produce exactly these bytes, and does it
// refuse exactly these malformed inputs".
//
// The fixtures are compiled in rather than read from the private JARs, so this
// stays sealed. `inflate_jar_test` is the companion that runs the same decoder
// over the user's real archives and compares against the build's own unpack --
// between them, correctness on known vectors and agreement with a trusted
// implementation on real data.
//
// The refusal cases matter as much as the success ones. This decoder reads a
// file the *user* supplied, so every malformed shape has to come back as false
// rather than as a read past the end of a buffer.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/common/platform/inflate.hpp"

namespace {

int failures = 0;

void check(bool ok, const char *what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

std::string text(const std::vector<uint8_t> &b) {
    return std::string((const char *)b.data(), b.size());
}

std::vector<uint8_t> raw(const uint8_t *p, size_t n) {
    return std::vector<uint8_t>(p, p + n);
}

// -------------------------------------------------------------- fixtures
//
// Produced with Python's zlib at default settings and pasted here, so the
// expected output is fixed by an independent implementation rather than by this
// one. `zlib.compress(data)[2:-4]` strips the zlib header and adler checksum,
// leaving the raw DEFLATE stream this decoder consumes.

// "hello hello hello hello" -- exercises a back-reference with an overlapping
// copy, which is how DEFLATE encodes runs and the case a memcpy would corrupt.
const uint8_t kHello[] = {0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0xc8, 0x40,
                          0x27, 0x01};
const char kHelloText[] = "hello hello hello hello";

// 200 'a's -- a long run from a one-byte window, the extreme of the same case.
const uint8_t kRun[] = {0x4b, 0x4c, 0x1c, 0x1e, 0x00, 0x00};

// A stored (uncompressed) block: type 0, with len and its complement.
const uint8_t kStored[] = {0x01, 0x05, 0x00, 0xfa, 0xff, 'p', 'l', 'a', 'i', 'n'};

void testInflate() {
    std::vector<uint8_t> out;

    check(platform::inflateRaw(kHello, sizeof(kHello), std::strlen(kHelloText), &out),
          "inflate: fixed-huffman stream decodes");
    check(text(out) == kHelloText, "inflate: and matches byte for byte");

    out.clear();
    check(platform::inflateRaw(kRun, sizeof(kRun), 200, &out),
          "inflate: long overlapping run decodes");
    check(out.size() == 200, "inflate: run has the right length");
    bool allA = true;
    for (size_t i = 0; i < out.size(); ++i) {
        if (out[i] != 'a') allA = false;
    }
    check(allA, "inflate: overlapping copy produced the right bytes");

    out.clear();
    check(platform::inflateRaw(kStored, sizeof(kStored), 5, &out),
          "inflate: stored block decodes");
    check(text(out) == "plain", "inflate: stored block bytes match");

    // An expected size that disagrees with the stream is an inconsistent
    // archive, and trusting either number over the other would be a guess.
    out.clear();
    check(!platform::inflateRaw(kHello, sizeof(kHello), 5, &out),
          "inflate: short expected size refused");
    out.clear();
    check(!platform::inflateRaw(kHello, sizeof(kHello), 500, &out),
          "inflate: long expected size refused");

    // Truncation must be reported, not silently accepted as a short result.
    out.clear();
    check(!platform::inflateRaw(kHello, sizeof(kHello) / 2, std::strlen(kHelloText), &out),
          "inflate: truncated stream refused");

    // Type 3 is reserved and must be rejected rather than guessed at.
    const uint8_t reserved[] = {0x07};
    out.clear();
    check(!platform::inflateRaw(reserved, sizeof(reserved), 4, &out),
          "inflate: reserved block type refused");

    // A stored block whose complement does not match is corruption.
    uint8_t badStored[sizeof(kStored)];
    std::memcpy(badStored, kStored, sizeof(kStored));
    badStored[3] ^= 0xFF;
    out.clear();
    check(!platform::inflateRaw(badStored, sizeof(badStored), 5, &out),
          "inflate: stored block with a bad complement refused");

    out.clear();
    check(!platform::inflateRaw(nullptr, 0, 4, &out), "inflate: empty input refused");
}

// ------------------------------------------------------------------- zip
//
// A minimal zip built by hand, so the reader is tested against a structure
// whose every offset is known. Both entries are stored, which keeps the fixture
// readable; the deflate path is covered above and against the real JARs.

void put16(std::vector<uint8_t> *v, uint16_t n) {
    v->push_back((uint8_t)n);
    v->push_back((uint8_t)(n >> 8));
}

void put32(std::vector<uint8_t> *v, uint32_t n) {
    v->push_back((uint8_t)n);
    v->push_back((uint8_t)(n >> 8));
    v->push_back((uint8_t)(n >> 16));
    v->push_back((uint8_t)(n >> 24));
}

void putStr(std::vector<uint8_t> *v, const std::string &s) {
    v->insert(v->end(), s.begin(), s.end());
}

struct Plan {
    std::string name;
    std::string body;
};

std::vector<uint8_t> makeZip(const std::vector<Plan> &files) {
    std::vector<uint8_t> zip;
    std::vector<uint32_t> offsets;

    for (size_t i = 0; i < files.size(); ++i) {
        offsets.push_back((uint32_t)zip.size());
        put32(&zip, 0x04034b50);
        put16(&zip, 20);  // version needed
        put16(&zip, 0);   // flags
        put16(&zip, 0);   // method: stored
        put16(&zip, 0);   // time
        put16(&zip, 0);   // date
        put32(&zip, 0);   // crc, unchecked by this reader
        put32(&zip, (uint32_t)files[i].body.size());
        put32(&zip, (uint32_t)files[i].body.size());
        put16(&zip, (uint16_t)files[i].name.size());
        put16(&zip, 0);  // extra
        putStr(&zip, files[i].name);
        putStr(&zip, files[i].body);
    }

    uint32_t cdStart = (uint32_t)zip.size();
    for (size_t i = 0; i < files.size(); ++i) {
        put32(&zip, 0x02014b50);
        put16(&zip, 20);  // version made by
        put16(&zip, 20);  // version needed
        put16(&zip, 0);   // flags
        put16(&zip, 0);   // method
        put16(&zip, 0);
        put16(&zip, 0);
        put32(&zip, 0);
        put32(&zip, (uint32_t)files[i].body.size());
        put32(&zip, (uint32_t)files[i].body.size());
        put16(&zip, (uint16_t)files[i].name.size());
        put16(&zip, 0);  // extra
        put16(&zip, 0);  // comment
        put16(&zip, 0);  // disk
        put16(&zip, 0);  // internal attrs
        put32(&zip, 0);  // external attrs
        put32(&zip, offsets[i]);
        putStr(&zip, files[i].name);
    }
    uint32_t cdSize = (uint32_t)zip.size() - cdStart;

    put32(&zip, 0x06054b50);
    put16(&zip, 0);
    put16(&zip, 0);
    put16(&zip, (uint16_t)files.size());
    put16(&zip, (uint16_t)files.size());
    put32(&zip, cdSize);
    put32(&zip, cdStart);
    put16(&zip, 0);  // comment length
    return zip;
}

void testZip() {
    std::vector<Plan> files;
    Plan a;
    a.name = "META-INF/MANIFEST.MF";
    a.body = "Manifest-Version: 1.0\r\n";
    Plan b;
    b.name = "datfiles.lmp";
    b.body = "ARCHIVE-BYTES";
    Plan dir;
    dir.name = "META-INF/";
    dir.body = "";
    files.push_back(a);
    files.push_back(b);
    files.push_back(dir);

    std::vector<uint8_t> zip = makeZip(files);
    std::vector<platform::ZipEntry> entries;
    std::string error;
    check(platform::readZip(zip, &entries, &error), "zip: well-formed archive reads");
    check(entries.size() == 2, "zip: directory entry skipped, files kept");
    if (entries.size() == 2) {
        check(entries[0].name == "META-INF/MANIFEST.MF", "zip: first entry name");
        check(text(entries[0].data) == a.body, "zip: first entry bytes");
        check(entries[1].name == "datfiles.lmp", "zip: second entry name");
        check(text(entries[1].data) == b.body, "zip: second entry bytes");
    }

    // Not a zip at all.
    std::vector<uint8_t> junk;
    for (int i = 0; i < 100; ++i) junk.push_back((uint8_t)i);
    entries.clear();
    check(!platform::readZip(junk, &entries, &error), "zip: non-archive refused");
    check(!error.empty(), "zip: refusal explains itself");

    // Too small to hold even an end-of-central-directory record.
    entries.clear();
    check(!platform::readZip(raw((const uint8_t *)"PK", 2), &entries, &error),
          "zip: tiny input refused");

    // A central directory pointing past the end of the file.
    std::vector<uint8_t> broken = makeZip(files);
    broken[broken.size() - 6] = 0xFF;
    broken[broken.size() - 5] = 0xFF;
    entries.clear();
    check(!platform::readZip(broken, &entries, &error),
          "zip: out-of-range central directory refused");

    // An unsupported compression method is refused, not half-decoded.
    std::vector<uint8_t> bzipped = makeZip(files);
    // Method field of the first central header, at cdStart + 10.
    uint32_t cdStart = 0;
    for (size_t i = bzipped.size() - 22; i > 0; --i) {
        if (bzipped[i] == 0x50 && bzipped[i + 1] == 0x4b && bzipped[i + 2] == 0x05 &&
            bzipped[i + 3] == 0x06) {
            cdStart = (uint32_t)bzipped[i + 16] | ((uint32_t)bzipped[i + 17] << 8) |
                      ((uint32_t)bzipped[i + 18] << 16) |
                      ((uint32_t)bzipped[i + 19] << 24);
            break;
        }
    }
    bzipped[cdStart + 10] = 12;  // bzip2
    entries.clear();
    check(!platform::readZip(bzipped, &entries, &error),
          "zip: unsupported compression method refused");
    check(error.find("unsupported") != std::string::npos,
          "zip: and says so rather than blaming the data");
}

}  // namespace

int main() {
    testInflate();
    testZip();

    if (failures != 0) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("inflate: all checks passed\n");
    return 0;
}
