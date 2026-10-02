// SPDX-License-Identifier: MIT
//
// Correctness tests. Every algorithm is checked against the standard library
// on edge cases and on every benchmark distribution. No framework needed.

#include <aao/search.hpp>
#include <aao/sort.hpp>

#include "inputs.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok && ++g_failures <= 50) std::fprintf(stderr, "FAIL: %s\n", what.c_str());
}

struct Counter {
    std::uint64_t* count;
    template <class A, class B>
    bool operator()(const A& a, const B& b) const {
        ++*count;
        return a < b;
    }
};

#define SORT_ENTRY(fn) {#fn, [](std::vector<int>& v) { fn(v.begin(), v.end()); }}

const std::vector<std::pair<std::string, void (*)(std::vector<int>&)>> kSorts = {
    SORT_ENTRY(aao::insertion_sort),
    SORT_ENTRY(aao::heap_sort),
    SORT_ENTRY(aao::textbook::merge_sort),
    SORT_ENTRY(aao::merge_sort),
    SORT_ENTRY(aao::textbook::quick_sort),
    SORT_ENTRY(aao::quick_sort),
    SORT_ENTRY(aao::intro_sort),
    SORT_ENTRY(aao::radix_sort),
};

std::vector<std::vector<int>> edge_cases() {
    constexpr int lo = std::numeric_limits<int>::min();
    constexpr int hi = std::numeric_limits<int>::max();
    return {
        {},
        {1},
        {2, 1},
        {1, 2},
        {3, 1, 2},
        {5, 5, 5, 5, 5, 5, 5},
        {hi, lo, 0, -1, 1, hi, lo},
        {-3, 7, -3, 0, 7, -100, 42, 0, 1, -1},
    };
}

void test_sorts() {
    std::vector<std::vector<int>> inputs = edge_cases();
    for (std::size_t n : {3u, 10u, 24u, 25u, 26u, 100u, 777u, 4096u})
        for (auto dist : bench::kSortDists) inputs.push_back(bench::make_sort_input(dist, n));

    for (const auto& [name, sort] : kSorts) {
        for (const auto& input : inputs) {
            auto expected = input;
            std::sort(expected.begin(), expected.end());
            auto got = input;
            sort(got);
            check(got == expected, name + " sorts n=" + std::to_string(input.size()));
        }
    }
}

void test_custom_comparator_and_types() {
    std::vector<std::string> words = {"pear", "apple", "fig", "banana", "kiwi", "cherry",
                                      "date", "grape", "lime", "mango", "nectarine", "olive",
                                      "papaya", "quince", "raspberry", "strawberry", "tomato",
                                      "ugli", "vanilla", "watermelon", "xigua", "yam", "zucchini",
                                      "apricot", "blueberry", "cantaloupe", "durian", "elderberry"};
    auto expected = words;
    std::sort(expected.begin(), expected.end(), std::greater<>{});
    auto run = [&](const char* name, auto sort) {
        auto got = words;
        sort(got.begin(), got.end(), std::greater<>{});
        check(got == expected, std::string(name) + " with std::greater on strings");
    };
    run("insertion_sort", [](auto f, auto l, auto c) { aao::insertion_sort(f, l, c); });
    run("heap_sort", [](auto f, auto l, auto c) { aao::heap_sort(f, l, c); });
    run("textbook::merge_sort", [](auto f, auto l, auto c) { aao::textbook::merge_sort(f, l, c); });
    run("merge_sort", [](auto f, auto l, auto c) { aao::merge_sort(f, l, c); });
    run("textbook::quick_sort", [](auto f, auto l, auto c) { aao::textbook::quick_sort(f, l, c); });
    run("quick_sort", [](auto f, auto l, auto c) { aao::quick_sort(f, l, c); });
    run("intro_sort", [](auto f, auto l, auto c) { aao::intro_sort(f, l, c); });
}

void test_stability() {
    // Sort (key, original position) pairs by key only; a stable sort keeps
    // equal keys in their original order.
    std::vector<std::pair<int, int>> items(3000);
    bench::Rng rng(1);
    for (int i = 0; i < 3000; ++i) items[i] = {static_cast<int>(rng.below(20)), i};
    auto by_key = [](const auto& a, const auto& b) { return a.first < b.first; };
    auto expected = items;
    std::stable_sort(expected.begin(), expected.end(), by_key);

    auto a = items;
    aao::insertion_sort(a.begin(), a.end(), by_key);
    check(a == expected, "insertion_sort is stable");
    auto b = items;
    aao::textbook::merge_sort(b.begin(), b.end(), by_key);
    check(b == expected, "textbook::merge_sort is stable");
    auto c = items;
    aao::merge_sort(c.begin(), c.end(), by_key);
    check(c == expected, "merge_sort is stable");
}

void test_radix_types() {
    auto run = [](auto sample) {
        using T = typename decltype(sample)::value_type;
        std::vector<T> v = sample;
        bench::Rng rng(3);
        for (int i = 0; i < 5000; ++i) v.push_back(static_cast<T>(rng.next()));
        auto expected = v;
        std::sort(expected.begin(), expected.end());
        aao::radix_sort(v.begin(), v.end());
        return v == expected;
    };
    using L8 = std::numeric_limits<std::int8_t>;
    using L64 = std::numeric_limits<std::int64_t>;
    using LU = std::numeric_limits<std::uint32_t>;
    check(run(std::vector<std::int8_t>{L8::min(), L8::max(), 0, -1}), "radix_sort int8_t");
    check(run(std::vector<std::uint16_t>{0, 65535}), "radix_sort uint16_t");
    check(run(std::vector<std::uint32_t>{LU::min(), LU::max()}), "radix_sort uint32_t");
    check(run(std::vector<std::int64_t>{L64::min(), L64::max(), 0, -1}), "radix_sort int64_t");
}

// The adversary must actually defeat aao::quick_sort, and intro_sort must
// shrug it off. Comparison counts make this a deterministic test.
void test_adversary_and_introsort_guarantee() {
    constexpr int n = 4000;
    const auto killer = bench::make_sort_input(bench::SortDist::adversarial, n);
    const auto random = bench::make_sort_input(bench::SortDist::random, n);

    auto comparisons = [](std::vector<int> v, auto sort) {
        std::uint64_t count = 0;
        sort(v.begin(), v.end(), Counter{&count});
        return count;
    };
    auto quick = [](auto f, auto l, auto c) { aao::quick_sort(f, l, c); };
    auto intro = [](auto f, auto l, auto c) { aao::intro_sort(f, l, c); };

    const double nlogn = n * std::log2(n);
    const auto quick_killer = comparisons(killer, quick);
    const auto quick_random = comparisons(random, quick);
    const auto intro_killer = comparisons(killer, intro);
    check(quick_killer > std::uint64_t{n} * n / 8,
          "adversary drives quick_sort quadratic (" + std::to_string(quick_killer) + " comparisons)");
    check(static_cast<double>(quick_random) < 2 * nlogn, "quick_sort is n log n on random input");
    check(static_cast<double>(intro_killer) < 4 * nlogn,
          "intro_sort stays n log n on the adversary (" + std::to_string(intro_killer) + " comparisons)");
}

void test_searches() {
    bench::Rng rng(5);
    for (int n : {0, 1, 2, 3, 7, 8, 15, 16, 17, 100, 1000}) {
        for (int range : {3, 50, 1 << 20}) {
            std::vector<int> v(static_cast<std::size_t>(n));
            for (auto& x : v) x = static_cast<int>(rng.below(static_cast<std::uint64_t>(range))) - range / 2;
            std::sort(v.begin(), v.end());
            const aao::eytzinger_index<int> eytz(v.begin(), v.end());
            const std::string ctx = " n=" + std::to_string(n) + " range=" + std::to_string(range);

            std::vector<int> keys;
            for (int x : v) keys.insert(keys.end(), {x - 1, x, x + 1});
            keys.insert(keys.end(), {std::numeric_limits<int>::min(), std::numeric_limits<int>::max(),
                                     -range, range});

            for (int key : keys) {
                const auto expected = std::lower_bound(v.begin(), v.end(), key);
                check(aao::branchless_lower_bound(v.begin(), v.end(), key) == expected,
                      "branchless_lower_bound" + ctx);
                check(aao::textbook::interpolation_search(v.begin(), v.end(), key) == expected,
                      "textbook::interpolation_search" + ctx);
                check(aao::interpolation_search(v.begin(), v.end(), key) == expected,
                      "interpolation_search" + ctx);

                const int* e = eytz.lower_bound(key);
                check(expected == v.end() ? e == nullptr : (e != nullptr && *e == *expected),
                      "eytzinger_index::lower_bound" + ctx);

                const auto found = aao::textbook::binary_search(v.begin(), v.end(), key);
                const bool present = std::binary_search(v.begin(), v.end(), key);
                check(present ? (found != v.end() && *found == key) : found == v.end(),
                      "textbook::binary_search" + ctx);
            }
        }
    }
}

// Regressions for bugs in v1's interpolation search: integer overflow on wide
// key ranges, and division by zero on runs of equal keys.
void test_interpolation_edge_cases() {
    constexpr int lo = std::numeric_limits<int>::min();
    constexpr int hi = std::numeric_limits<int>::max();
    const std::vector<int> wide = {lo, lo + 1, -5, 0, 7, hi - 1, hi};
    for (int key : {lo, lo + 1, -6, -5, 0, 1, 7, 8, hi - 1, hi}) {
        const auto expected = std::lower_bound(wide.begin(), wide.end(), key);
        check(aao::textbook::interpolation_search(wide.begin(), wide.end(), key) == expected,
              "interpolation on the full int range, key=" + std::to_string(key));
        check(aao::interpolation_search(wide.begin(), wide.end(), key) == expected,
              "guarded interpolation on the full int range, key=" + std::to_string(key));
    }
    const std::vector<int> flat(1000, 42);
    for (int key : {41, 42, 43}) {
        const auto expected = std::lower_bound(flat.begin(), flat.end(), key);
        check(aao::textbook::interpolation_search(flat.begin(), flat.end(), key) == expected,
              "interpolation on all-equal keys, key=" + std::to_string(key));
    }
    const std::vector<double> reals = {-1.5, 0.0, 0.25, 0.25, 3.75, 1e300};
    for (double key : {-2.0, -1.5, 0.1, 0.25, 4.0, 1e300, 2e300}) {
        const auto expected = std::lower_bound(reals.begin(), reals.end(), key);
        check(aao::interpolation_search(reals.begin(), reals.end(), key) == expected,
              "interpolation on doubles");
    }
}

}  // namespace

int main() {
    test_sorts();
    test_custom_comparator_and_types();
    test_stability();
    test_radix_types();
    test_adversary_and_introsort_guarantee();
    test_searches();
    test_interpolation_edge_cases();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
