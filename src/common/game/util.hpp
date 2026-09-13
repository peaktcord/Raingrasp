#ifndef COMMON_GAME_UTIL_HPP
#define COMMON_GAME_UTIL_HPP

#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"

class GameUtil {
public:
    static std::string ensureLeadingSlash(const std::string &name);
    static BinaryReader *openResource(platform::PlatformContext *context,
                                      const std::string &name);
    static BinaryReader *openDatFile(platform::PlatformContext *context,
                                     const std::string &name);

    static int32_t randomInt(GameRandom *random, int32_t bound);

    static std::string coordKey(int32_t x, int32_t y);
    static std::string replace(const std::string &text, const std::string &find, int32_t value);
    static std::string replace(const std::string &text, const std::string &find, const std::string &value);
    static std::string replace(const std::string &text, const std::string &find, SharedArray<std::string> values);
    static SharedArray<std::string> splitWhitespace(const std::string &text);

    static int32_t setBit(int32_t bit, int32_t bits);
    static int32_t clearBit(int32_t bit, int32_t bits);
    static int8_t setFlag(int8_t flag, int8_t flags);
    static int8_t clearFlag(int8_t flag, int8_t flags);
    static bool hasFlag(int8_t flag, int8_t flags);

    static int64_t readLongBE(const SharedArray<int8_t> &bytes, int32_t offset);

};

#endif
