#ifndef COMMON_PLATFORM_CRASH_TRACE_HPP
#define COMMON_PLATFORM_CRASH_TRACE_HPP

namespace crash_trace {

void install();

// Installs a stack dump at the throw site of the runtime's bounds checks.
// The game catches its own exceptions, so the ordinary crash filter never sees
// them and the frames are gone by the time the catch logs.  Debug builds only:
// walking the stack is far too slow for a throw on a hot path.
void installThrowTrace();

}

#endif
