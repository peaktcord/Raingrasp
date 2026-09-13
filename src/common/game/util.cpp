#include "src/common/game/util.hpp"

std::string GameUtil::ensureLeadingSlash(const std::string &name) {
    if (name.rfind("/", 0) == 0) {
        return name;
    }
    return std::string("/") + name;
}

BinaryReader *GameUtil::openResource(platform::PlatformContext *context,
                                     const std::string &name) {
    if (context == nullptr) {
        return nullptr;
    }
    std::vector<uint8_t> bytes;
    if (!context->readResource(ensureLeadingSlash(name), &bytes)) return nullptr;
    return new BinaryReader(std::move(bytes));
}

BinaryReader *GameUtil::openDatFile(platform::PlatformContext *context,
                                    const std::string &name) {
    return openResource(context, name);
}

int32_t GameUtil::randomInt(GameRandom *random, int32_t bound) {
    return 1 + wrappingAbs(random->nextInt() % bound);
}

std::string GameUtil::coordKey(int32_t x, int32_t y) {
    return std::to_string(x) + "," + std::to_string(y);
}

std::string GameUtil::replace(const std::string &text, const std::string &find, int32_t value) {
    return replace(text, find, std::to_string(value));
}

std::string GameUtil::replace(const std::string &text, const std::string &find, const std::string &value) {
    size_t at = text.find(find);
    if (at == std::string::npos) {
        return text;
    }
    std::string before = text.substr(0, at);
    std::string after = text.substr(at + find.length());
    return before + value + after;
}

std::string GameUtil::replace(const std::string &text, const std::string &find, SharedArray<std::string> values) {
    std::string result = text;
    int32_t n = values.length();
    int32_t at = 0;
    while (at < n) {
        result = replace(result, find, values[at]);
        ++at;
    }
    return result;
}

int32_t GameUtil::setBit(int32_t bit, int32_t bits) { return bits | (1 << bit); }

int32_t GameUtil::clearBit(int32_t bit, int32_t bits) { return bits & ~(1 << bit); }

int8_t GameUtil::setFlag(int8_t flag, int8_t flags) { return (int8_t)(flags | flag); }

int8_t GameUtil::clearFlag(int8_t flag, int8_t flags) { return (int8_t)(flags & ~flag); }

bool GameUtil::hasFlag(int8_t flag, int8_t flags) { return (flags & flag) != 0; }

int64_t GameUtil::readLongBE(const SharedArray<int8_t> &bytes, int32_t offset) {
    int64_t value = 0;
    int32_t n = 0;
    while (n < 8) {
        int64_t part = bytes[n + offset] & 0xFF;
        value |= part << ((7 - n) * 8);
        ++n;
    }
    return value;
}

SharedArray<std::string> GameUtil::splitWhitespace(const std::string &text) {
    size_t first = text.find_first_not_of(" \t\n\r");
    std::string string;
    if (first != std::string::npos) {
        size_t last = text.find_last_not_of(" \t\n\r");
        string = text.substr(first, last - first + 1);
    }
    int32_t n = 1;
    int32_t n2 = string.length();
    bool bl = false;
    int32_t n3 = 0;
    while (n3 < n2) {
        if (string[(size_t)n3] == ' ') {
            if (!bl) {
                ++n;
                bl = true;
            }
        } else {
            bl = false;
        }
        ++n3;
    }
    SharedArray<std::string> stringArray(n);
    int32_t n4 = 0;
    int32_t n5 = 0;
    int32_t n6 = 0;
    while (n6 < n2) {
        if (string[(size_t)n6] == ' ') {
            if (!bl) {
                stringArray[n5++] = string.substr((size_t)n4, (size_t)(n6 - n4));
                n4 = n6 + 1;
                bl = true;
            } else {
                ++n4;
            }
        } else {
            bl = false;
        }
        ++n6;
    }
    stringArray[n5] = string.substr((size_t)n4, (size_t)(n2 - n4));
    return stringArray;
}
