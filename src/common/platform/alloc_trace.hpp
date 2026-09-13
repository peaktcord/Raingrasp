#ifndef COMMON_PLATFORM_ALLOC_TRACE_HPP
#define COMMON_PLATFORM_ALLOC_TRACE_HPP

#include <cstddef>

namespace alloc_trace {

void start();

void stop();

void report(const char *label, int topSites = 8);

std::size_t liveBytes();
std::size_t liveBlocks();

}

#endif
