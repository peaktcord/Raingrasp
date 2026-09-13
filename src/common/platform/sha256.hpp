#ifndef COMMON_PLATFORM_SHA256_HPP
#define COMMON_PLATFORM_SHA256_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace platform {

std::string sha256Hex(const uint8_t *data, size_t size);
std::string sha256Hex(const std::vector<uint8_t> &data);

std::string contentDigest(
    std::vector<std::pair<std::string, const std::vector<uint8_t> *>> entries);

}

#endif
