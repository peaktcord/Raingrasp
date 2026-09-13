#ifndef COMMON_RENDER_FRAME_CHECK_HPP
#define COMMON_RENDER_FRAME_CHECK_HPP

#include <string>

#include "src/common/render/render.hpp"

namespace framecheck {

struct Result {
    bool compared = false;
    bool match = false;
    bool sizeMismatch = false;
    double changedPct = 0.0;
    double structurePct = 0.0;
    std::string note;
};

Result compare(const render::Surface &frame, const std::string &baselineDir,
               const std::string &name, const std::string &failDir);

std::string describe(const Result &result);

}

#endif
