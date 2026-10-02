// SPDX-License-Identifier: MIT
//
// aao/search.hpp - searching a sorted range, from textbook to tuned.
//
// All lower-bound style functions return the first element that is not less
// than `key` (the same contract as std::lower_bound), or `last` if there is
// none. The range must be sorted with respect to the same ordering.

#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <new>
#include <type_traits>
#include <vector>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <xmmintrin.h>
#endif

namespace aao {

namespace detail {

// Hint the CPU to start loading `addr` into cache. Never faults, so it may
// point anywhere, including past the end of an array. (Deliberately not
// noexcept: on SJLJ-exception toolchains such as some MinGW builds, an inlined
// noexcept body makes the caller register an exception frame on every call.)
inline void prefetch(std::uintptr_t addr) {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(reinterpret_cast<const void*>(addr));
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    _mm_prefetch(reinterpret_cast<const char*>(addr), _MM_HINT_T0);
#else
    (void)addr;
#endif
}

}  // namespace detail

// ---------------------------------------------------------------------------
// Binary search
// ---------------------------------------------------------------------------

namespace textbook {

/// Classic binary search with an early exit on equality, as found in most
/// textbooks. Returns an iterator to *an* element equal to `key`, or `last`.
/// About log2(n) iterations, each with a hard-to-predict branch.
template <class It, class T, class Comp = std::less<>>
It binary_search(It first, It last, const T& key, Comp comp = {}) {
    It lo = first, hi = last;
    while (lo < hi) {
        const It mid = lo + (hi - lo) / 2;
        if (comp(*mid, key)) lo = mid + 1;
        else if (comp(key, *mid)) hi = mid;
        else return mid;
    }
    return last;
}

}  // namespace textbook

/// Branchless lower bound. The loop always runs ceil(log2(n)) times and its
/// only data-dependent step compiles to a conditional move, so there are no
/// branch mispredictions to pay for (a branchy binary search mispredicts about
/// half of its branches, ~15-20 cycles each). Both candidates for the next
/// probe are prefetched, which hides part of the memory latency on large arrays.
template <class It, class T, class Comp = std::less<>>
It branchless_lower_bound(It first, It last, const T& key, Comp comp = {}) {
    auto len = last - first;
    if (len == 0) return last;
    It base = first;
    while (len > 1) {
        const auto half = len / 2;
        if constexpr (std::contiguous_iterator<It>) {
            // The next probe is at base + next - 1 or base + half + next - 1.
            // Plain integer arithmetic: the address may fall outside the array
            // on the last iteration, which is harmless for a prefetch.
            using V = typename std::iterator_traits<It>::value_type;
            const auto addr = reinterpret_cast<std::uintptr_t>(std::to_address(base));
            const auto next = static_cast<std::uintptr_t>((len - half) / 2);
            detail::prefetch(addr + (next - 1) * sizeof(V));
            detail::prefetch(addr + (static_cast<std::uintptr_t>(half) + next - 1) * sizeof(V));
        }
        // Arithmetic rather than `cond ? a : b`: GCC turns the ternary into a
        // branch, but reliably emits a conditional move for this form.
        base += static_cast<decltype(len)>(comp(base[half - 1], key)) * half;
        len -= half;
    }
    return comp(*base, key) ? base + 1 : base;
}

// ---------------------------------------------------------------------------
// Interpolation search
// ---------------------------------------------------------------------------

namespace detail {

// Where linear interpolation between (lo, lo_v) and (hi, hi_v) expects `key`.
// Computed in double: integer arithmetic overflows on wide key ranges.
// Requires lo_v < key <= hi_v. Returns an offset in [0, span].
template <class V, class T, class D>
D interpolate(const V& lo_v, const V& hi_v, const T& key, D span) {
    const double den = static_cast<double>(hi_v) - static_cast<double>(lo_v);
    if (!(den > 0)) return span / 2;  // values too close to tell apart in double
    const double frac = (static_cast<double>(key) - static_cast<double>(lo_v)) / den;
    return std::clamp(static_cast<D>(frac * static_cast<double>(span)), D{0}, span);
}

}  // namespace detail

namespace textbook {

/// Interpolation search (lower-bound contract). Instead of probing the middle
/// it guesses where `key` should be from the values at both ends.
/// O(log log n) probes on uniformly distributed keys, but O(n) on skewed
/// ones: on exponential-like data each guess moves the bound by one element.
template <class It, class T>
It interpolation_search(It first, It last, const T& key) {
    It lo = first, hi = last;  // the answer lies in [lo, hi]
    while (lo < hi) {
        if (!(*lo < key)) return lo;
        if (*(hi - 1) < key) return hi;
        // Now *lo < key <= *(hi - 1), so the denominator below is positive.
        const It probe = lo + detail::interpolate(*lo, *(hi - 1), key, (hi - 1) - lo);
        if (*probe < key) lo = probe + 1;
        else hi = probe;
    }
    return lo;
}

}  // namespace textbook

/// Guarded interpolation search: alternates one interpolation probe with one
/// bisection step. On uniform data it keeps the O(log log n) behaviour of
/// interpolation search; on any data it needs at most ~2*log2(n) probes,
/// so skewed inputs can no longer push it to O(n).
template <class It, class T>
It interpolation_search(It first, It last, const T& key) {
    It lo = first, hi = last;  // the answer lies in [lo, hi]
    while (lo < hi) {
        if (!(*lo < key)) return lo;
        if (*(hi - 1) < key) return hi;
        const It probe = lo + detail::interpolate(*lo, *(hi - 1), key, (hi - 1) - lo);
        if (*probe < key) lo = probe + 1;
        else hi = probe;
        if (lo >= hi) break;
        const It mid = lo + (hi - lo) / 2;  // bisection step bounds the worst case
        if (*mid < key) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

// ---------------------------------------------------------------------------
// Eytzinger layout
// ---------------------------------------------------------------------------

/// A sorted array rearranged into Eytzinger (breadth-first, heap) order:
/// the root at index 1, the children of node k at 2k and 2k+1.
///
/// Binary search over a sorted array touches a new, far-away cache line on
/// almost every step once the array outgrows the cache. In this layout the
/// first levels of the tree share a handful of cache lines, and the 16
/// great-great-grandchildren of any node are contiguous, so one prefetch per
/// step fetches the data needed four iterations later. On arrays larger than
/// the last-level cache this is typically 2-4x faster than std::lower_bound.
template <class T, class Comp = std::less<>>
class eytzinger_index {
public:
    eytzinger_index() = default;

    /// Builds the layout from a sorted range in O(n).
    template <class It>
    eytzinger_index(It first, It last, Comp comp = {})
        : n_(static_cast<std::size_t>(last - first)), tree_(n_ + 1), comp_(comp) {
        build(first, 1);
    }

    [[nodiscard]] std::size_t size() const noexcept { return n_; }

    /// The smallest element not less than `key`, or nullptr if every element is less.
    [[nodiscard]] const T* lower_bound(const T& key) const {
        const T* t = tree_.data();
        const auto base = reinterpret_cast<std::uintptr_t>(t);
        std::size_t k = 1;
        while (k <= n_) {
            detail::prefetch(base + k * kPrefetchStride * sizeof(T));
            k = 2 * k + (comp_(t[k], key) ? 1 : 0);
        }
        // k now encodes the path taken: each 1-bit is a "go right". The answer
        // is the last node where we went left, i.e. k with its trailing ones
        // and one more bit shifted out. k == 0 means we never went left.
        k >>= std::countr_one(k) + 1;
        return k == 0 ? nullptr : t + k;
    }

private:
    // Nodes 16k .. 16k+15 are the descendants of k four levels down; with
    // 4-byte keys they fill exactly one 64-byte cache line.
    static constexpr std::size_t kPrefetchStride = std::bit_floor(std::size_t{64} / sizeof(T)) > 0
                                                       ? std::bit_floor(std::size_t{64} / sizeof(T))
                                                       : 1;

    // In-order traversal of the implicit tree consumes the sorted input in order.
    template <class It>
    It build(It it, std::size_t k) {
        if (k <= n_) {
            it = build(it, 2 * k);
            tree_[k] = *it++;
            it = build(it, 2 * k + 1);
        }
        return it;
    }

    // Cache-line aligned storage, so that node 16k starts a cache line.
    template <class U>
    struct aligned_allocator {
        using value_type = U;
        aligned_allocator() = default;
        template <class V>
        constexpr aligned_allocator(const aligned_allocator<V>&) noexcept {}
        U* allocate(std::size_t count) {
            return static_cast<U*>(::operator new(count * sizeof(U), std::align_val_t{64}));
        }
        void deallocate(U* p, std::size_t) noexcept { ::operator delete(p, std::align_val_t{64}); }
        template <class V>
        bool operator==(const aligned_allocator<V>&) const noexcept { return true; }
    };

    std::size_t n_ = 0;
    std::vector<T, aligned_allocator<T>> tree_ = std::vector<T, aligned_allocator<T>>(1);
    Comp comp_{};
};

}  // namespace aao
