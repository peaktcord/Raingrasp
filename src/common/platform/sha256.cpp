#include "src/common/platform/sha256.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace platform {

namespace {

const uint32_t kK[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

struct Sha256 {
    uint32_t h[8];
    uint64_t bits;
    uint8_t block[64];
    size_t fill;

    Sha256() : bits(0), fill(0) {
        h[0] = 0x6a09e667u;
        h[1] = 0xbb67ae85u;
        h[2] = 0x3c6ef372u;
        h[3] = 0xa54ff53au;
        h[4] = 0x510e527fu;
        h[5] = 0x9b05688cu;
        h[6] = 0x1f83d9abu;
        h[7] = 0x5be0cd19u;
        std::memset(block, 0, sizeof(block));
    }

    void compress(const uint8_t *p) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
                   ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + s1 + ch + kK[i] + w[i];
            uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = s0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    void update(const uint8_t *data, size_t size) {
        bits += (uint64_t)size * 8;
        while (size > 0) {
            size_t take = std::min(sizeof(block) - fill, size);
            std::memcpy(block + fill, data, take);
            fill += take;
            data += take;
            size -= take;
            if (fill == sizeof(block)) {
                compress(block);
                fill = 0;
            }
        }
    }

    void finish(uint8_t out[32]) {
        uint64_t total = bits;
        uint8_t pad = 0x80;
        update(&pad, 1);
        bits = total;
        uint8_t zero = 0;
        while (fill != 56) update(&zero, 1);
        bits = total;

        uint8_t tail[8];
        for (int i = 0; i < 8; ++i) tail[i] = (uint8_t)(total >> (56 - i * 8));
        std::memcpy(block + fill, tail, 8);
        compress(block);

        for (int i = 0; i < 8; ++i) {
            out[i * 4] = (uint8_t)(h[i] >> 24);
            out[i * 4 + 1] = (uint8_t)(h[i] >> 16);
            out[i * 4 + 2] = (uint8_t)(h[i] >> 8);
            out[i * 4 + 3] = (uint8_t)h[i];
        }
    }
};

std::string hex(const uint8_t digest[32]) {
    static const char *digits = "0123456789abcdef";
    std::string out;
    out.resize(64);
    for (int i = 0; i < 32; ++i) {
        out[i * 2] = digits[digest[i] >> 4];
        out[i * 2 + 1] = digits[digest[i] & 0xF];
    }
    return out;
}

}

std::string sha256Hex(const uint8_t *data, size_t size) {
    Sha256 state;
    state.update(data, size);
    uint8_t digest[32];
    state.finish(digest);
    return hex(digest);
}

std::string sha256Hex(const std::vector<uint8_t> &data) {
    return sha256Hex(data.empty() ? (const uint8_t *)"" : data.data(), data.size());
}

std::string contentDigest(
    std::vector<std::pair<std::string, const std::vector<uint8_t> *>> entries) {
    std::sort(entries.begin(), entries.end(),
              [](const std::pair<std::string, const std::vector<uint8_t> *> &a,
                 const std::pair<std::string, const std::vector<uint8_t> *> &b) {
                  return a.first < b.first;
              });

    Sha256 tree;
    for (size_t i = 0; i < entries.size(); ++i) {
        const std::vector<uint8_t> &payload = *entries[i].second;
        uint8_t member[32];
        {
            Sha256 one;
            one.update(payload.empty() ? (const uint8_t *)"" : payload.data(),
                       payload.size());
            one.finish(member);
        }
        tree.update((const uint8_t *)entries[i].first.data(), entries[i].first.size());
        uint8_t nul = 0;
        tree.update(&nul, 1);
        uint8_t size[8];
        uint64_t n = (uint64_t)payload.size();
        for (int b = 0; b < 8; ++b) size[b] = (uint8_t)(n >> (56 - b * 8));
        tree.update(size, 8);
        tree.update(member, 32);
    }

    uint8_t digest[32];
    tree.finish(digest);
    return hex(digest);
}

}
