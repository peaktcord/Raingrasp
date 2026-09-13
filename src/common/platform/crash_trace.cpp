#include "src/common/platform/crash_trace.hpp"

#include "src/common/platform/platform.hpp"

#include <windows.h>
#include <dbghelp.h>

#include <cstdarg>
#include <cstdio>

namespace crash_trace {

namespace {

// The shipping build is a windowed process, so stderr goes nowhere the player
// can reach.  Mirror every crash line into the session log
// (%LOCALAPPDATA%/Raingrasp/raingrasp.log) so a report can be ingested after
// the fact.  Everything here runs on a faulted thread: no allocation beyond a
// fixed buffer, and each line is flushed as it is written.
void report(const char *format, ...) {
    char line[2048];
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (written < 0) return;

    std::fputs(line, stderr);
    std::fflush(stderr);
    if (std::FILE *log = platform::logFile()) {
        std::fputs(line, log);
        std::fflush(log);
    }
}

const char *exceptionName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "access violation";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
        case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
        case 0xC0000409: return "stack buffer overrun (/GS)";
        default: return "exception";
    }
}

void printStack(CONTEXT *context, int skipFrames = 0) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (!SymInitialize(process, nullptr, TRUE)) {
        report("  (no symbols: SymInitialize failed, %lu)\n", GetLastError());
    }

    STACKFRAME64 frame{};
    DWORD machine;
#if defined(_M_X64)
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context->Rip;
    frame.AddrFrame.Offset = context->Rbp;
    frame.AddrStack.Offset = context->Rsp;
#elif defined(_M_IX86)
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context->Eip;
    frame.AddrFrame.Offset = context->Ebp;
    frame.AddrStack.Offset = context->Esp;
#else
#error "crash_trace: unsupported architecture"
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO) + 1024] = {};
    SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *>(symbolBuffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 1024;

    for (int walked = 0, depth = 0; walked < 64 + skipFrames; ++walked) {
        if (!StackWalk64(machine, process, thread, &frame, context, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            break;
        }
        if (frame.AddrPC.Offset == 0) break;
        // Drop the trace plumbing itself, so frame #0 is the code that failed.
        if (walked < skipFrames) continue;
        const int reported = depth++;

        const DWORD64 pc = frame.AddrPC.Offset;
        DWORD64 displacement = 0;
        const char *name = "??";
        if (SymFromAddr(process, pc, &displacement, symbol)) name = symbol->Name;

        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, pc, &lineDisplacement, &line)) {
            report("  #%-2d %s\n        %s:%lu\n", reported, name,
                         line.FileName, line.LineNumber);
        } else {
            report("  #%-2d %s + 0x%llx\n", reported, name,
                         (unsigned long long)displacement);
        }
    }
    SymCleanup(process);
}

LONG WINAPI onCrash(EXCEPTION_POINTERS *info) {
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    std::fflush(stdout);
    SYSTEMTIME now{};
    GetLocalTime(&now);
    report("\n=== crash: %s (0x%08lx) at %p ===\n", exceptionName(code),
                 (unsigned long)code, info->ExceptionRecord->ExceptionAddress);
    report("    %04u-%02u-%02u %02u:%02u:%02u  pid %lu thread %lu\n", now.wYear,
           now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
           GetCurrentProcessId(), GetCurrentThreadId());
    if (code == EXCEPTION_ACCESS_VIOLATION &&
        info->ExceptionRecord->NumberParameters >= 2) {
        const ULONG_PTR kind = info->ExceptionRecord->ExceptionInformation[0];
        report("    %s address 0x%llx\n",
                     kind == 1 ? "writing" : kind == 8 ? "executing" : "reading",
                     (unsigned long long)info->ExceptionRecord->ExceptionInformation[1]);
    }
    printStack(info->ContextRecord);
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

// Called from the throw site, before the throw.  The game catches its own
// exceptions, so this is the only moment the guilty frames still exist.
void onThrow(const char *what) {
    // Re-entry would be a throw from inside the walk itself; drop it rather
    // than recurse.  Not thread-safe by design: the game ticks on one thread,
    // and a lock here could deadlock against whatever the throw interrupted.
    static bool tracing = false;
    if (tracing) return;
    tracing = true;

    SYSTEMTIME now{};
    GetLocalTime(&now);
    report("\n=== throw: %s ===\n", what != nullptr ? what : "(no message)");
    report("    %04u-%02u-%02u %02u:%02u:%02u  thread %lu\n", now.wYear,
           now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
           GetCurrentThreadId());

    CONTEXT context{};
    RtlCaptureContext(&context);
    // Hide RtlCaptureContext's frame, this function, and the two platform
    // shims between it and the code that actually indexed out of bounds.
    printStack(&context, 3);
    tracing = false;
}

}

void install() { SetUnhandledExceptionFilter(onCrash); }

void installThrowTrace() { platform::setThrowTraceHook(onThrow); }

}
