#ifndef COMMON_GAME_TEXTWRAP_HPP
#define COMMON_GAME_TEXTWRAP_HPP

#include <vector>

#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"

namespace textwrap {

enum class TextWrap {
    Slack,
    Words,
};

std::vector<std::string> wrapSlack(Font *font, int32_t width, const std::string &text);
std::vector<std::string> wrapWords(Font *font, int32_t width, const std::string &text);

std::vector<std::string> wrap(TextWrap how, Font *font, int32_t width, const std::string &text);

}

#endif
