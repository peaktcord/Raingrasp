#ifndef COMMON_PLATFORM_INFLATE_HPP
#define COMMON_PLATFORM_INFLATE_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace platform {

bool inflateRaw(const uint8_t *data, size_t size, size_t expected,
                std::vector<uint8_t> *out);

struct ZipEntry {
    std::string name;
    std::vector<uint8_t> data;
};

bool readZip(const std::vector<uint8_t> &bytes, std::vector<ZipEntry> *out,
             std::string *error);

}

#endif
