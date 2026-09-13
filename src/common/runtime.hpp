#ifndef COMMON_RUNTIME_HPP
#define COMMON_RUNTIME_HPP

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/common/platform/platform.hpp"

inline int32_t wrappingAbs(int32_t n) { return n < 0 ? -n : n; }
inline int32_t min32(int32_t a, int32_t b) { return a < b ? a : b; }
inline int32_t max32(int32_t a, int32_t b) { return a > b ? a : b; }
inline int32_t shiftLeft32(int32_t v, int32_t s) { return (int32_t)((uint32_t)v << (s & 31)); }
inline int32_t unsignedShiftRight32(int32_t v, int32_t s) { return (int32_t)((uint32_t)v >> (s & 31)); }
inline int64_t unsignedShiftRight64(int64_t v, int32_t s) { return (int64_t)((uint64_t)v >> (s & 63)); }

template <typename T>
class SharedArray {
    std::shared_ptr<std::vector<T>> v_;
public:
    SharedArray() {}
    explicit SharedArray(int32_t n) : v_(std::make_shared<std::vector<T>>((size_t)n, T())) {}
    SharedArray(std::initializer_list<T> il) : v_(std::make_shared<std::vector<T>>(il)) {}
    bool isNull() const { return !v_; }
    void setNull() { v_.reset(); }
    int32_t length() const { return (int32_t)v_->size(); }
    // Log the stack before unwinding it.  The catch that reports a bad index
    // is far above the subscript that made one, so the message alone never
    // names the array; a debug build installs the hook and gets the frames.
    // Cold and never inlined so the bounds test stays a predictable branch.
    [[noreturn]] static void throwBadIndex(const std::string &message) {
        platform::traceThrowSite(message);
        throw std::out_of_range(message);
    }

    // Java threw on a bad index; the straight vector subscript this replaced
    // read past the buffer instead and handed back a plausible-looking value.
    // GameCanvas::tick catches std::exception, so a stray index now surfaces
    // as a logged error screen rather than as a corrupted pointer.
    T &operator[](int32_t i) const {
        if (v_ == nullptr) {
            throwBadIndex("SharedArray: index " + std::to_string(i) +
                          " into a null array");
        }
        if (i < 0 || (size_t)i >= v_->size()) {
            throwBadIndex("SharedArray: index " + std::to_string(i) +
                          " out of bounds for length " +
                          std::to_string(v_->size()));
        }
        return (*v_)[(size_t)i];
    }
    bool sameRef(const SharedArray &o) const { return v_ == o.v_; }
    const void *refId() const { return v_.get(); }
};

template <typename T>
SharedArray<SharedArray<T>> makeSharedArray2D(int32_t rows, int32_t cols) {
    SharedArray<SharedArray<T>> a(rows);
    for (int32_t i = 0; i < rows; ++i) a[i] = SharedArray<T>(cols);
    return a;
}

template <typename T>
void copySharedArray(const SharedArray<T> &src, int32_t sp, SharedArray<T> &dst, int32_t dp, int32_t n) {
    if (src.sameRef(dst) && dp > sp) {
        for (int32_t i = n - 1; i >= 0; --i) dst[dp + i] = src[sp + i];
    } else {
        for (int32_t i = 0; i < n; ++i) dst[dp + i] = src[sp + i];
    }
}

class GameRandom {
    uint64_t seed_;
    int32_t next(int bits) {
        seed_ = (seed_ * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1);
        return (int32_t)(seed_ >> (48 - bits));
    }
public:
    explicit GameRandom(int64_t seed) { setSeed(seed); }
    void setSeed(int64_t s) { seed_ = ((uint64_t)s ^ 0x5DEECE66DULL) & ((1ULL << 48) - 1); }
    int32_t nextInt() { return next(32); }
};

#endif
