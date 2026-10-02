// SPDX-License-Identifier: MIT
//
// Deterministic benchmark inputs. The same (distribution, n, seed) produces
// the same array on every compiler and platform, so results are comparable
// across machines.

#pragma once

#include <aao/sort.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string_view>
#include <utility>
#include <vector>

namespace bench {

// SplitMix64. Unlike std::uniform_int_distribution, its output is specified
// exactly, so inputs do not differ between libstdc++, libc++ and MSVC.
struct Rng {
    std::uint64_t state;

    explicit Rng(std::uint64_t seed) : state(seed) {}

    std::uint64_t next() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // Uniform integer in [0, bound) for bound <= 2^32 (multiply-shift, no division).
    std::uint64_t below(std::uint64_t bound) { return ((next() >> 32) * bound) >> 32; }

    // Uniform double in [0, 1).
    double unit() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }
};

// ---------------------------------------------------------------------------
// Sorting inputs
// ---------------------------------------------------------------------------

enum class SortDist { random, sorted, reversed, nearly_sorted, few_unique, adversarial };

inline constexpr std::array kSortDists = {SortDist::random,        SortDist::sorted,
                                          SortDist::reversed,      SortDist::nearly_sorted,
                                          SortDist::few_unique,    SortDist::adversarial};

constexpr std::string_view name(SortDist d) {
    switch (d) {
        case SortDist::random: return "random";
        case SortDist::sorted: return "sorted";
        case SortDist::reversed: return "reversed";
        case SortDist::nearly_sorted: return "nearly_sorted";
        case SortDist::few_unique: return "few_unique";
        case SortDist::adversarial: return "adversarial";
    }
    return "?";
}

/// McIlroy's "killer adversary" (Software: Practice and Experience, 1999).
/// Runs `sort` on indices with a comparator that decides element values
/// lazily, always so that the pivot candidate ends up near an extreme. The
/// values it settles on form an input that drives that exact (deterministic)
/// quicksort to quadratic time. Costs as much as the sort it defeats.
template <class Sort>
std::vector<int> quicksort_killer(int n, Sort sort) {
    const int gas = n - 1;  // "not yet decided"; every decided value is smaller
    std::vector<int> val(static_cast<std::size_t>(n), gas);
    int solid = 0;
    int candidate = 0;
    auto freeze = [&](int x) { val[static_cast<std::size_t>(x)] = solid++; };
    auto comp = [&](int x, int y) {
        auto& vx = val[static_cast<std::size_t>(x)];
        auto& vy = val[static_cast<std::size_t>(y)];
        if (vx == gas && vy == gas) freeze(x == candidate ? x : y);
        if (vx == gas) candidate = x;
        else if (vy == gas) candidate = y;
        return vx < vy;
    };
    std::vector<int> idx(static_cast<std::size_t>(n));
    std::iota(idx.begin(), idx.end(), 0);
    sort(idx.begin(), idx.end(), comp);
    return val;
}

inline std::vector<int> make_sort_input(SortDist dist, std::size_t n, std::uint64_t seed = 42) {
    Rng rng(seed ^ (static_cast<std::uint64_t>(dist) << 56) ^ n);
    std::vector<int> v(n);
    switch (dist) {
        case SortDist::random:
            for (auto& x : v) x = static_cast<int>(static_cast<std::uint32_t>(rng.next()));
            break;
        case SortDist::sorted:
            std::iota(v.begin(), v.end(), 0);
            break;
        case SortDist::reversed:
            std::iota(v.rbegin(), v.rend(), 0);
            break;
        case SortDist::nearly_sorted:  // sorted, then 1% of positions swapped at random
            std::iota(v.begin(), v.end(), 0);
            if (n >= 2)
                for (std::size_t i = 0; i < std::max<std::size_t>(1, n / 100); ++i) {
                    const auto a = rng.below(n);  // two statements: argument evaluation
                    const auto b = rng.below(n);  // order would differ between compilers
                    std::swap(v[a], v[b]);
                }
            break;
        case SortDist::few_unique:  // 16 distinct values
            for (auto& x : v) x = static_cast<int>(rng.below(16));
            break;
        case SortDist::adversarial:
            v = quicksort_killer(static_cast<int>(n),
                                 [](auto f, auto l, auto c) { aao::quick_sort(f, l, c); });
            break;
    }
    return v;
}

// ---------------------------------------------------------------------------
// Searching inputs
// ---------------------------------------------------------------------------

enum class SearchDist { uniform, skewed };

inline constexpr std::array kSearchDists = {SearchDist::uniform, SearchDist::skewed};

constexpr std::string_view name(SearchDist d) {
    return d == SearchDist::uniform ? "uniform" : "skewed";
}

/// A sorted array of n keys in [0, 2^31), generated directly in order.
///   uniform: one random key per equal-width bucket - evenly spread values.
///   skewed:  2^31 * u^8 for sorted uniform u - most keys crowd near zero,
///            the pathological case for interpolation search.
inline std::vector<int> make_search_input(SearchDist dist, std::size_t n, std::uint64_t seed = 7) {
    Rng rng(seed ^ (static_cast<std::uint64_t>(dist) << 56) ^ n);
    std::vector<int> v(n);
    constexpr std::int64_t kRange = std::int64_t{1} << 31;
    if (dist == SearchDist::uniform) {
        const std::int64_t bucket = std::max<std::int64_t>(1, kRange / static_cast<std::int64_t>(n));
        for (std::size_t i = 0; i < n; ++i)
            v[i] = static_cast<int>(std::min<std::int64_t>(
                kRange - 1, static_cast<std::int64_t>(i) * bucket +
                                static_cast<std::int64_t>(rng.below(static_cast<std::uint64_t>(bucket)))));
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            const double u = (static_cast<double>(i) + rng.unit()) / static_cast<double>(n);
            v[i] = static_cast<int>(std::pow(u, 8.0) * static_cast<double>(kRange - 1));
        }
    }
    return v;
}

/// Query keys drawn from the array itself, so every search succeeds.
inline std::vector<int> make_queries(const std::vector<int>& data, std::size_t count,
                                     std::uint64_t seed = 99) {
    Rng rng(seed ^ data.size());
    std::vector<int> q(count);
    for (auto& k : q) k = data[rng.below(data.size())];
    return q;
}

}  // namespace bench
