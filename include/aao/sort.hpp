// SPDX-License-Identifier: MIT
//
// aao/sort.hpp - sorting algorithms, from textbook to tuned.
//
// Every function sorts the half-open range [first, last) in place, ordered by
// `comp` (default: operator<), and accepts any random-access iterator.
//
//   aao::textbook::*   the versions you find in textbooks (and in v1 of this
//                      repository): correct, but with the classic weaknesses.
//   aao::*             the same ideas after measurement-driven optimization.
//
// The benchmark in bench/ measures every rung of that ladder; the README and
// docs/analysis.md explain why each step helps.

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

namespace aao {

namespace detail {

template <class It>
using value_t = typename std::iterator_traits<It>::value_type;

template <class It>
using diff_t = typename std::iterator_traits<It>::difference_type;

// Below this length insertion sort beats every divide-and-conquer sort here:
// no recursion, predictable branches, and the whole range sits in L1 cache.
// Measured with aao_bench on a Ryzen 9800X3D: raising it from 16 to 32 makes
// random input 5-10% faster; 64 gains ~3% more on random input but nearly
// doubles merge_sort's time on reversed input (insertion sort's worst case).
inline constexpr std::ptrdiff_t kSmallSort = 32;

}  // namespace detail

// ---------------------------------------------------------------------------
// Insertion sort
// ---------------------------------------------------------------------------

/// Insertion sort. O(n^2) worst case, O(n) on already-sorted input, stable.
/// Slow in general, but the fastest option for tiny ranges, which is why the
/// tuned sorts below hand every small subproblem to it.
template <class It, class Comp = std::less<>>
void insertion_sort(It first, It last, Comp comp = {}) {
    if (last - first < 2) return;
    for (It i = first + 1; i != last; ++i) {
        auto key = std::move(*i);
        It j = i;
        for (; j != first && comp(key, *(j - 1)); --j) *j = std::move(*(j - 1));
        *j = std::move(key);
    }
}

// ---------------------------------------------------------------------------
// Heap sort
// ---------------------------------------------------------------------------

namespace detail {

template <class It, class Comp>
void sift_down(It first, diff_t<It> root, diff_t<It> n, Comp& comp) {
    auto value = std::move(first[root]);
    for (;;) {
        diff_t<It> child = 2 * root + 1;
        if (child >= n) break;
        if (child + 1 < n && comp(first[child], first[child + 1])) ++child;
        if (!comp(value, first[child])) break;
        first[root] = std::move(first[child]);
        root = child;
    }
    first[root] = std::move(value);
}

}  // namespace detail

/// Heap sort. O(n log n) worst case with O(1) extra space; not stable.
/// Its scattered memory access makes it 2-3x slower than quicksort in
/// practice, so it mostly serves as the safety net inside intro_sort.
template <class It, class Comp = std::less<>>
void heap_sort(It first, It last, Comp comp = {}) {
    const auto n = last - first;
    if (n < 2) return;
    for (auto i = n / 2 - 1; i >= 0; --i) detail::sift_down(first, i, n, comp);
    for (auto end = n - 1; end > 0; --end) {
        std::iter_swap(first, first + end);
        detail::sift_down(first, detail::diff_t<It>{0}, end, comp);
    }
}

// ---------------------------------------------------------------------------
// Merge sort
// ---------------------------------------------------------------------------

namespace textbook {

/// Top-down merge sort as most textbooks write it: recurse all the way down to
/// single elements and allocate two fresh vectors for every merge.
/// O(n log n) in every case and stable, but the ~n allocations dominate.
template <class It, class Comp = std::less<>>
void merge_sort(It first, It last, Comp comp = {}) {
    const auto n = last - first;
    if (n < 2) return;
    const It mid = first + n / 2;
    merge_sort(first, mid, comp);
    merge_sort(mid, last, comp);

    std::vector<detail::value_t<It>> left(first, mid), right(mid, last);
    auto l = left.begin(), r = right.begin();
    It out = first;
    while (l != left.end() && r != right.end()) *out++ = comp(*r, *l) ? *r++ : *l++;
    out = std::copy(l, left.end(), out);
    std::copy(r, right.end(), out);
}

}  // namespace textbook

namespace detail {

// Merges the sorted runs [first, mid) and [mid, last). Only the left run is
// moved out to `buf`; the right run is consumed in place, so the buffer needs
// floor(n/2) slots rather than n.
template <class It, class T, class Comp>
void merge_runs(It first, It mid, It last, T* buf, Comp& comp) {
    T* const buf_end = std::move(first, mid, buf);
    T* l = buf;
    It r = mid, out = first;
    while (l != buf_end && r != last) {
        if (comp(*r, *l)) *out++ = std::move(*r++);
        else *out++ = std::move(*l++);
    }
    std::move(l, buf_end, out);  // leftovers of the right run are already in place
}

template <class It, class T, class Comp>
void merge_sort_impl(It first, It last, T* buf, Comp& comp) {
    const auto n = last - first;
    if (n <= kSmallSort) {
        insertion_sort(first, last, comp);
        return;
    }
    const It mid = first + n / 2;
    merge_sort_impl(first, mid, buf, comp);
    merge_sort_impl(mid, last, buf, comp);
    if (!comp(*mid, *(mid - 1))) return;  // runs already in order: nothing to merge
    merge_runs(first, mid, last, buf, comp);
}

}  // namespace detail

/// Merge sort, optimized. Three changes over textbook::merge_sort:
///   1. one scratch buffer of n/2 elements, allocated once;
///   2. ranges of <= 32 elements are finished with insertion sort;
///   3. a merge is skipped when the two runs are already in order,
///      which makes sorted input O(n).
/// Still O(n log n) worst case and stable.
template <class It, class Comp = std::less<>>
void merge_sort(It first, It last, Comp comp = {}) {
    const auto n = last - first;
    if (n < 2) return;
    std::vector<detail::value_t<It>> buf(static_cast<std::size_t>(n / 2));
    detail::merge_sort_impl(first, last, buf.data(), comp);
}

// ---------------------------------------------------------------------------
// Quicksort
// ---------------------------------------------------------------------------

namespace textbook {

/// Quicksort with the Lomuto partition and the last element as pivot, as in
/// CLRS. Sorted, reversed and all-equal inputs make every partition maximally
/// unbalanced: O(n^2) time and O(n) recursion depth.
template <class It, class Comp = std::less<>>
void quick_sort(It first, It last, Comp comp = {}) {
    if (last - first < 2) return;
    const It pivot = last - 1;
    It store = first;
    for (It j = first; j != pivot; ++j)
        if (comp(*j, *pivot)) std::iter_swap(store++, j);
    std::iter_swap(store, pivot);
    quick_sort(first, store, comp);
    quick_sort(store + 1, last, comp);
}

}  // namespace textbook

namespace detail {

// Orders *a <= *b <= *c.
template <class It, class Comp>
void sort3(It a, It b, It c, Comp& comp) {
    if (comp(*b, *a)) std::iter_swap(a, b);
    if (comp(*c, *b)) {
        std::iter_swap(b, c);
        if (comp(*b, *a)) std::iter_swap(a, b);
    }
}

// Median-of-three pivot + Hoare partition. Requires last - first >= 3.
// Returns `cut` such that every element of [first, cut) is <= every element
// of [cut, last), with both sides non-empty.
//
// After sort3, *first <= pivot <= *(last - 1), so the two scans need no
// bounds checks: each is stopped by the opposite sentinel. Elements equal to
// the pivot are swapped too, which keeps the split balanced on inputs with
// many duplicates (where Lomuto degrades to O(n^2)).
template <class It, class Comp>
It partition_hoare(It first, It last, Comp& comp) {
    const It mid = first + (last - first) / 2;
    sort3(first, mid, last - 1, comp);
    const auto pivot = *mid;
    It i = first, j = last - 1;
    for (;;) {
        do ++i; while (comp(*i, pivot));
        do --j; while (comp(pivot, *j));
        if (i >= j) return i;
        std::iter_swap(i, j);
    }
}

// depth_limit < 0 means unlimited (plain quicksort).
template <class It, class Comp>
void quick_sort_impl(It first, It last, Comp& comp, int depth_limit) {
    while (last - first > kSmallSort) {
        if (depth_limit == 0) {
            heap_sort(first, last, comp);
            return;
        }
        if (depth_limit > 0) --depth_limit;
        const It cut = partition_hoare(first, last, comp);
        // Recurse into the smaller side and loop on the larger one, so the
        // stack never holds more than log2(n) frames.
        if (cut - first < last - cut) {
            quick_sort_impl(first, cut, comp, depth_limit);
            first = cut;
        } else {
            quick_sort_impl(cut, last, comp, depth_limit);
            last = cut;
        }
    }
    insertion_sort(first, last, comp);
}

}  // namespace detail

/// Quicksort, optimized: median-of-three pivot, Hoare partition, insertion
/// sort below 32 elements, and recursion on the smaller side only (O(log n)
/// stack). Fast on every common input, but still O(n^2) on inputs built to
/// defeat median-of-three (see the "adversarial" benchmark distribution).
template <class It, class Comp = std::less<>>
void quick_sort(It first, It last, Comp comp = {}) {
    detail::quick_sort_impl(first, last, comp, -1);
}

/// Introsort (Musser, 1997): quick_sort that watches its own recursion depth
/// and switches to heap_sort for any range that gets deeper than 2*log2(n).
/// Quicksort speed on normal inputs, O(n log n) guaranteed on all of them.
/// This is the strategy behind std::sort in libstdc++, libc++ and MSVC.
template <class It, class Comp = std::less<>>
void intro_sort(It first, It last, Comp comp = {}) {
    const auto n = last - first;
    if (n < 2) return;
    const int depth_limit = 2 * (std::bit_width(static_cast<std::uint64_t>(n)) - 1);
    detail::quick_sort_impl(first, last, comp, depth_limit);
}

// ---------------------------------------------------------------------------
// Radix sort
// ---------------------------------------------------------------------------

/// LSD radix sort for integer keys. It never compares two elements: it makes
/// one counting pass per byte of the key (4 for 32-bit integers), so it runs
/// in O(n * sizeof(T)) time and sidesteps the O(n log n) lower bound that
/// binds every comparison sort. Passes in which all keys share the same byte
/// are skipped. Needs O(n) extra memory; stable.
///
/// Known weak spot: when all 256 buckets of a pass are exactly the same size
/// (e.g. the keys 0..n-1 with n a power of two), the 256 write positions move
/// in lockstep a power-of-two distance apart, land in the same cache sets and
/// evict each other: about 4x slower than on random keys. Software
/// write-combining (buffering a cache line per bucket) is the usual fix.
template <class It>
void radix_sort(It first, It last) {
    using T = detail::value_t<It>;
    static_assert(std::is_integral_v<T>, "radix_sort sorts integer keys");
    using U = std::make_unsigned_t<T>;
    constexpr int kBytes = sizeof(T);
    // Flipping the sign bit maps signed order onto unsigned order.
    constexpr U kFlip = std::is_signed_v<T> ? U(U(1) << (kBytes * 8 - 1)) : U(0);

    const auto n = static_cast<std::size_t>(last - first);
    if (n < 2) return;

    std::vector<U> a(n), b(n);
    std::transform(first, last, a.begin(), [](T x) { return U(U(x) ^ kFlip); });

    std::array<std::array<std::size_t, 256>, kBytes> counts{};
    for (const U x : a)
        for (int p = 0; p < kBytes; ++p) ++counts[p][(x >> (8 * p)) & 0xFF];

    U* src = a.data();
    U* dst = b.data();
    for (int p = 0; p < kBytes; ++p) {
        auto& count = counts[p];
        if (count[(src[0] >> (8 * p)) & 0xFF] == n) continue;  // byte is constant
        std::size_t offset = 0;
        for (auto& c : count) offset += std::exchange(c, offset);  // exclusive prefix sum
        for (std::size_t i = 0; i < n; ++i) dst[count[(src[i] >> (8 * p)) & 0xFF]++] = src[i];
        std::swap(src, dst);
    }
    std::transform(src, src + n, first, [](U x) { return static_cast<T>(U(x ^ kFlip)); });
}

}  // namespace aao
