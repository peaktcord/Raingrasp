#include "src/common/game/textwrap.hpp"
#include "src/common/game/util.hpp"

namespace textwrap {

namespace {

int32_t indexOf(const std::string &text, char needle, int32_t from = 0) {
    size_t at = text.find(needle, (size_t)from);
    return at == std::string::npos ? -1 : (int32_t)at;
}

}

std::vector<std::string> wrapSlack(Font *font, int32_t n, const std::string &stringIn) {
    int32_t n1;
    int32_t n2 = 0;
    std::string string1 = stringIn;
    n2 = indexOf(string1, '\n');
    if (n2 >= 0) {
        if (n2 == string1.length() - 1) {
            string1 = string1.substr(0, string1.length() - 1);
        } else {
            std::vector<std::string> lines =
                n2 == 0 ? std::vector<std::string>{std::string(" ")}
                        : wrapSlack(font, n, string1.substr(0, n2));
            std::vector<std::string> tail = wrapSlack(font, n, string1.substr(n2 + 1));
            lines.insert(lines.end(), tail.begin(), tail.end());
            return lines;
        }
    }
    if (font->stringWidth(string1) < n) {
        return {string1};
    }
    string1 = string1 + " ";
    std::vector<std::string> lines;
    int32_t n4 = 0;
    n -= 8;
    while ((n2 = indexOf(string1, ' ', n4 + 1)) > 0) {
        if (font->substringWidth(string1, 0, n2) < n) {
            n4 = n2;
            continue;
        }
        if (n4 == 0) {
            n1 = 0;
            while (n1 < n) {
                n1 += font->charWidth(string1[(size_t)n4]);
                ++n4;
            }
            lines.push_back(string1.substr(0, n4));
            --n4;
        } else {
            lines.push_back(string1.substr(0, n4));
        }
        string1 = string1.substr(n4 + 1);
        n4 = 0;
    }
    if (!string1.empty() && string1 != " ") {
        lines.push_back(string1);
    }
    return lines;
}

namespace {

std::vector<std::string> splitNewlines(const std::string &string) {
    std::vector<std::string> lines;
    int32_t n1 = 0;
    int32_t n2 = 0;
    do {
        if ((n2 = indexOf(string, '\n', n1)) < 0) {
            n2 = string.length();
            lines.push_back(string.substr((size_t)n1, (size_t)(n2 - n1)));
            break;
        }
        lines.push_back(string.substr((size_t)n1, (size_t)(n2 - n1)));
    } while ((n1 = n2 + 1) < string.length());
    return lines;
}

std::vector<std::string> breakLongWord(Font *font, int32_t n1, const std::string &string) {
    std::vector<std::string> lines;
    int32_t n2 = string.length();
    std::string string1("");
    int32_t n3 = 0;
    while (n3 < n2) {
        char c1 = string[(size_t)n3];
        std::string string2 = string1 + c1;
        if (font->stringWidth(string2) > n1) {
            lines.push_back(std::string(string1));
            string1.assign(1, c1);
        } else {
            string1 = string2;
        }
        ++n3;
    }
    lines.push_back(std::string(string1));
    return lines;
}

void wrapLine(Font *font, int32_t n1, std::vector<std::string> &lines, const std::string &string) {
    SharedArray<std::string> stringArray1 = GameUtil::splitWhitespace(string);
    int32_t n2 = stringArray1.length();
    std::string string1("");
    int32_t n3 = 0;
    while (n3 < n2) {
        std::string string2 = stringArray1[n3];
        std::string string3 = string1 + string2;
        if (font->stringWidth(string3) > n1) {
            if (font->stringWidth(string2) > n1) {
                if (string1.length() > 0) {
                    lines.push_back(std::string(string1));
                }
                std::vector<std::string> stringArray2 = breakLongWord(font, n1, string2);
                int32_t n4 = 0;
                while (n4 < (int32_t)stringArray2.size() - 1) {
                    lines.push_back(std::string(stringArray2[(size_t)n4]));
                    ++n4;
                }
                string3 = stringArray2.back();
            } else {
                lines.push_back(std::string(string1));
                string3 = string2;
            }
        }
        string1 = string3;
        if (n3 < n2 - 1) {
            string3 = string1 + " ";
            if (font->stringWidth(string3) > n1) {
                lines.push_back(std::string(string1));
                string1 = std::string("");
            } else {
                string1 = string3;
            }
        }
        ++n3;
    }
    lines.push_back(std::string(string1));
}

}

std::vector<std::string> wrapWords(Font *font, int32_t width, const std::string &text) {
    std::vector<std::string> lines;
    std::vector<std::string> stringArray1 = splitNewlines(text);
    int32_t n1 = (int32_t)stringArray1.size();
    int32_t n2 = 0;
    while (n2 < n1) {
        std::string string1 = stringArray1[(size_t)n2];
        wrapLine(font, width, lines, string1);
        ++n2;
    }
    return lines;
}

std::vector<std::string> wrap(TextWrap how, Font *font, int32_t width, const std::string &text) {
    return how == TextWrap::Slack ? wrapSlack(font, width, text) : wrapWords(font, width, text);
}

}
