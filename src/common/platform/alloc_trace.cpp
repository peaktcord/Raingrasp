#include "src/common/platform/alloc_trace.hpp"

#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace alloc_trace {

namespace {

const int kFrames = 12;

struct Site {
    void *frames[kFrames];
    int depth;
};

struct Live {
    std::size_t bytes;
    std::size_t site;
};

struct Totals {
    std::size_t bytes = 0;
    std::size_t blocks = 0;
};

template <typename T>
struct RawAlloc {
    using value_type = T;
    RawAlloc() = default;
    template <typename U>
    RawAlloc(const RawAlloc<U> &) {}
    T *allocate(std::size_t n) { return static_cast<T *>(std::malloc(n * sizeof(T))); }
    void deallocate(T *p, std::size_t) { std::free(p); }
    template <typename U>
    bool operator==(const RawAlloc<U> &) const { return true; }
    template <typename U>
    bool operator!=(const RawAlloc<U> &) const { return false; }
};

std::mutex &lock() {
    static std::mutex m;
    return m;
}

using SiteList = std::vector<Site, RawAlloc<Site>>;
using LiveMap = std::unordered_map<void *, Live, std::hash<void *>, std::equal_to<void *>,
                                   RawAlloc<std::pair<void *const, Live>>>;
using TotalMap = std::unordered_map<std::size_t, Totals, std::hash<std::size_t>,
                                    std::equal_to<std::size_t>,
                                    RawAlloc<std::pair<const std::size_t, Totals>>>;

SiteList &sites() {
    static SiteList s;
    return s;
}
LiveMap &live() {
    static LiveMap m;
    return m;
}
TotalMap &totals() {
    static TotalMap m;
    return m;
}

bool g_on = false;
thread_local bool t_inside = false;

std::size_t g_liveBytes = 0;
std::size_t g_liveBlocks = 0;

std::size_t recordSite() {
    Site s{};
    s.depth = (int)CaptureStackBackTrace(2, kFrames, s.frames, nullptr);
    SiteList &list = sites();
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i].depth == s.depth &&
            std::memcmp(list[i].frames, s.frames, sizeof(void *) * (std::size_t)s.depth) == 0) {
            return i;
        }
    }
    list.push_back(s);
    return list.size() - 1;
}

void note(void *p, std::size_t bytes) {
    if (!g_on || p == nullptr || t_inside) return;
    t_inside = true;
    {
        std::lock_guard<std::mutex> guard(lock());
        const std::size_t site = recordSite();
        live()[p] = Live{bytes, site};
        Totals &t = totals()[site];
        t.bytes += bytes;
        t.blocks += 1;
        g_liveBytes += bytes;
        g_liveBlocks += 1;
    }
    t_inside = false;
}

void forget(void *p) {
    if (p == nullptr || t_inside) return;
    t_inside = true;
    {
        std::lock_guard<std::mutex> guard(lock());
        LiveMap &m = live();
        const auto it = m.find(p);
        if (it != m.end()) {
            Totals &t = totals()[it->second.site];
            t.bytes -= it->second.bytes;
            t.blocks -= 1;
            g_liveBytes -= it->second.bytes;
            g_liveBlocks -= 1;
            m.erase(it);
        }
    }
    t_inside = false;
}

}

void start() {
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    std::lock_guard<std::mutex> guard(lock());
    g_on = true;
}

void stop() {
    std::lock_guard<std::mutex> guard(lock());
    g_on = false;
}

std::size_t liveBytes() { return g_liveBytes; }
std::size_t liveBlocks() { return g_liveBlocks; }

void report(const char *label, int topSites) {
    t_inside = true;
    std::lock_guard<std::mutex> guard(lock());

    std::vector<std::pair<std::size_t, Totals>, RawAlloc<std::pair<std::size_t, Totals>>>
        ranked;
    for (const auto &entry : totals()) {
        if (entry.second.blocks == 0) continue;
        ranked.push_back(entry);
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto &a, const auto &b) { return a.second.bytes > b.second.bytes; });

    std::printf("\n=== %s: %zu live blocks, %.1f MB ===\n", label, g_liveBlocks,
                (double)g_liveBytes / (1024.0 * 1024.0));

    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512] = {};
    SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 512;
    HANDLE process = GetCurrentProcess();

    const int shown = std::min<int>(topSites, (int)ranked.size());
    for (int n = 0; n < shown; ++n) {
        const Site &s = sites()[ranked[(std::size_t)n].first];
        const Totals &t = ranked[(std::size_t)n].second;
        std::printf("  %6.1f MB in %7zu blocks:\n", (double)t.bytes / (1024.0 * 1024.0),
                    t.blocks);
        for (int f = 0; f < s.depth && f < 6; ++f) {
            DWORD64 displacement = 0;
            const char *name = "??";
            if (SymFromAddr(process, (DWORD64)s.frames[f], &displacement, symbol)) {
                name = symbol->Name;
            }
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process, (DWORD64)s.frames[f], &lineDisplacement,
                                     &line)) {
                const char *file = std::strrchr(line.FileName, '\\');
                std::printf("      %s (%s:%lu)\n", name, file ? file + 1 : line.FileName,
                            line.LineNumber);
            } else {
                std::printf("      %s\n", name);
            }
        }
    }
    std::fflush(stdout);
    t_inside = false;
}

}

void *operator new(std::size_t bytes) {
    void *p = std::malloc(bytes == 0 ? 1 : bytes);
    if (p == nullptr) throw std::bad_alloc();
    alloc_trace::note(p, bytes);
    return p;
}

void *operator new[](std::size_t bytes) { return operator new(bytes); }

void operator delete(void *p) noexcept {
    alloc_trace::forget(p);
    std::free(p);
}

void operator delete[](void *p) noexcept { operator delete(p); }
void operator delete(void *p, std::size_t) noexcept { operator delete(p); }
void operator delete[](void *p, std::size_t) noexcept { operator delete(p); }
