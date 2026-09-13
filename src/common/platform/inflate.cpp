#include "src/common/platform/inflate.hpp"

#include <cstring>

namespace platform {

namespace {

struct BitReader {
    const uint8_t *data;
    size_t size;
    size_t pos = 0;
    uint32_t bitBuf = 0;
    int bitCount = 0;
    bool bad = false;

    BitReader(const uint8_t *d, size_t n) : data(d), size(n) {}

    int bit() {
        if (bitCount == 0) {
            if (pos >= size) {
                bad = true;
                return 0;
            }
            bitBuf = data[pos++];
            bitCount = 8;
        }
        int b = (int)(bitBuf & 1u);
        bitBuf >>= 1;
        --bitCount;
        return b;
    }

    uint32_t bits(int n) {
        uint32_t v = 0;
        for (int i = 0; i < n; ++i) v |= (uint32_t)bit() << i;
        return v;
    }

    void alignToByte() {
        bitBuf = 0;
        bitCount = 0;
    }
};

struct Huffman {
    int counts[16];
    std::vector<int> symbols;

    bool build(const uint8_t *lengths, int n) {
        std::memset(counts, 0, sizeof(counts));
        for (int i = 0; i < n; ++i) counts[lengths[i]]++;
        counts[0] = 0;

        int left = 1;
        for (int len = 1; len < 16; ++len) {
            left <<= 1;
            left -= counts[len];
            if (left < 0) return false;
        }

        int offsets[16];
        offsets[0] = 0;
        offsets[1] = 0;
        for (int len = 1; len < 15; ++len) offsets[len + 1] = offsets[len] + counts[len];

        symbols.assign((size_t)n, 0);
        for (int i = 0; i < n; ++i) {
            if (lengths[i] != 0) symbols[(size_t)offsets[lengths[i]]++] = i;
        }
        return true;
    }

    int decode(BitReader *br) const {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= br->bit();
            if (br->bad) return -1;
            int count = counts[len];
            if (code - first < count) {
                size_t at = (size_t)(index + (code - first));
                if (at >= symbols.size()) return -1;
                return symbols[at];
            }
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        return -1;
    }
};

const uint16_t kLengthBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const uint8_t kLengthExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const uint16_t kDistBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577};
const uint8_t kDistExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool inflateBlockData(BitReader *br, const Huffman &lit, const Huffman &dist,
                      size_t limit, std::vector<uint8_t> *out) {
    for (;;) {
        int sym = lit.decode(br);
        if (sym < 0) return false;
        if (sym < 256) {
            if (out->size() >= limit) return false;
            out->push_back((uint8_t)sym);
            continue;
        }
        if (sym == 256) return true;
        sym -= 257;
        if (sym >= 29) return false;
        size_t length = (size_t)kLengthBase[sym] + br->bits(kLengthExtra[sym]);

        int dsym = dist.decode(br);
        if (dsym < 0 || dsym >= 30) return false;
        size_t distance = (size_t)kDistBase[dsym] + br->bits(kDistExtra[dsym]);
        if (br->bad) return false;
        if (distance == 0 || distance > out->size()) return false;
        if (out->size() + length > limit) return false;

        size_t from = out->size() - distance;
        for (size_t i = 0; i < length; ++i) out->push_back((*out)[from + i]);
    }
}

bool buildFixedTables(Huffman *lit, Huffman *dist) {
    uint8_t lengths[288];
    for (int i = 0; i < 144; ++i) lengths[i] = 8;
    for (int i = 144; i < 256; ++i) lengths[i] = 9;
    for (int i = 256; i < 280; ++i) lengths[i] = 7;
    for (int i = 280; i < 288; ++i) lengths[i] = 8;
    if (!lit->build(lengths, 288)) return false;

    uint8_t dlengths[30];
    for (int i = 0; i < 30; ++i) dlengths[i] = 5;
    return dist->build(dlengths, 30);
}

bool buildDynamicTables(BitReader *br, Huffman *lit, Huffman *dist) {
    int hlit = (int)br->bits(5) + 257;
    int hdist = (int)br->bits(5) + 1;
    int hclen = (int)br->bits(4) + 4;
    if (br->bad || hlit > 288 || hdist > 30) return false;

    static const int kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                                   11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint8_t clen[19];
    std::memset(clen, 0, sizeof(clen));
    for (int i = 0; i < hclen; ++i) clen[kOrder[i]] = (uint8_t)br->bits(3);
    if (br->bad) return false;

    Huffman code;
    if (!code.build(clen, 19)) return false;

    std::vector<uint8_t> lengths((size_t)(hlit + hdist), 0);
    size_t n = 0;
    while (n < lengths.size()) {
        int sym = code.decode(br);
        if (sym < 0) return false;
        if (sym < 16) {
            lengths[n++] = (uint8_t)sym;
            continue;
        }
        int repeat = 0;
        uint8_t value = 0;
        if (sym == 16) {
            if (n == 0) return false;
            value = lengths[n - 1];
            repeat = 3 + (int)br->bits(2);
        } else if (sym == 17) {
            repeat = 3 + (int)br->bits(3);
        } else {
            repeat = 11 + (int)br->bits(7);
        }
        if (br->bad || n + (size_t)repeat > lengths.size()) return false;
        for (int i = 0; i < repeat; ++i) lengths[n++] = value;
    }

    if (!lit->build(lengths.data(), hlit)) return false;
    return dist->build(lengths.data() + hlit, hdist);
}

}

bool inflateRaw(const uint8_t *data, size_t size, size_t expected,
                std::vector<uint8_t> *out) {
    BitReader br(data, size);
    out->clear();
    out->reserve(expected);

    for (;;) {
        int final = br.bit();
        int type = (int)br.bits(2);
        if (br.bad) return false;

        if (type == 0) {
            br.alignToByte();
            if (br.pos + 4 > br.size) return false;
            uint32_t len = (uint32_t)data[br.pos] | ((uint32_t)data[br.pos + 1] << 8);
            uint32_t nlen = (uint32_t)data[br.pos + 2] | ((uint32_t)data[br.pos + 3] << 8);
            br.pos += 4;
            if ((len ^ 0xFFFFu) != nlen) return false;
            if (br.pos + len > br.size) return false;
            if (out->size() + len > expected) return false;
            out->insert(out->end(), data + br.pos, data + br.pos + len);
            br.pos += len;
        } else if (type == 1 || type == 2) {
            Huffman lit;
            Huffman dist;
            bool ok = (type == 1) ? buildFixedTables(&lit, &dist)
                                  : buildDynamicTables(&br, &lit, &dist);
            if (!ok) return false;
            if (!inflateBlockData(&br, lit, dist, expected, out)) return false;
        } else {
            return false;
        }

        if (final) break;
    }

    return out->size() == expected;
}

namespace {

uint16_t le16(const uint8_t *p) {
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

const uint32_t kEndOfCentralDir = 0x06054b50;
const uint32_t kCentralFileHeader = 0x02014b50;
const uint32_t kLocalFileHeader = 0x04034b50;

}

bool readZip(const std::vector<uint8_t> &bytes, std::vector<ZipEntry> *out,
             std::string *error) {
    if (bytes.size() < 22) {
        *error = "not a zip archive: too small";
        return false;
    }

    size_t eocd = 0;
    bool found = false;
    size_t maxComment = bytes.size() < 65557 ? bytes.size() : 65557;
    for (size_t back = 22; back <= maxComment; ++back) {
        size_t at = bytes.size() - back;
        if (le32(&bytes[at]) == kEndOfCentralDir) {
            eocd = at;
            found = true;
            break;
        }
    }
    if (!found) {
        *error = "not a zip archive: no end-of-central-directory record";
        return false;
    }

    uint16_t count = le16(&bytes[eocd + 10]);
    uint32_t cdSize = le32(&bytes[eocd + 12]);
    uint32_t cdOffset = le32(&bytes[eocd + 16]);
    if ((size_t)cdOffset + cdSize > bytes.size()) {
        *error = "zip: central directory extends past the end of the file";
        return false;
    }

    size_t pos = cdOffset;
    for (uint16_t i = 0; i < count; ++i) {
        if (pos + 46 > bytes.size() || le32(&bytes[pos]) != kCentralFileHeader) {
            *error = "zip: malformed central directory entry";
            return false;
        }
        uint16_t method = le16(&bytes[pos + 10]);
        uint32_t compressed = le32(&bytes[pos + 20]);
        uint32_t uncompressed = le32(&bytes[pos + 24]);
        uint16_t nameLen = le16(&bytes[pos + 28]);
        uint16_t extraLen = le16(&bytes[pos + 30]);
        uint16_t commentLen = le16(&bytes[pos + 32]);
        uint32_t localOffset = le32(&bytes[pos + 42]);
        if (pos + 46 + nameLen > bytes.size()) {
            *error = "zip: entry name extends past the end of the file";
            return false;
        }
        std::string name((const char *)&bytes[pos + 46], nameLen);
        pos += 46u + nameLen + extraLen + commentLen;

        if (!name.empty() && (name[name.size() - 1] == '/' ||
                              name[name.size() - 1] == '\\')) {
            continue;
        }

        if ((size_t)localOffset + 30 > bytes.size() ||
            le32(&bytes[localOffset]) != kLocalFileHeader) {
            *error = "zip: bad local header for " + name;
            return false;
        }
        uint16_t localNameLen = le16(&bytes[localOffset + 26]);
        uint16_t localExtraLen = le16(&bytes[localOffset + 28]);
        size_t dataAt = (size_t)localOffset + 30 + localNameLen + localExtraLen;
        if (dataAt + compressed > bytes.size()) {
            *error = "zip: entry data extends past the end of the file: " + name;
            return false;
        }

        ZipEntry entry;
        entry.name = name;
        if (method == 0) {
            if (compressed != uncompressed) {
                *error = "zip: stored entry has mismatched sizes: " + name;
                return false;
            }
            entry.data.assign(bytes.begin() + (long)dataAt,
                              bytes.begin() + (long)(dataAt + compressed));
        } else if (method == 8) {
            if (!inflateRaw(&bytes[dataAt], compressed, uncompressed, &entry.data)) {
                *error = "zip: could not inflate " + name;
                return false;
            }
        } else {
            *error = "zip: unsupported compression method for " + name;
            return false;
        }
        out->push_back(entry);
    }

    return true;
}

}
