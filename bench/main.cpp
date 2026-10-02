// SPDX-License-Identifier: MIT
//
// aao_bench - measures every algorithm in include/aao on every input
// distribution and writes the results as CSV.
//
//   aao_bench [sort|search|all] [--quick] [--out DIR] [--max-n N]
//             [--filter TEXT] [--trace FILE]
//
// Method: inputs are generated deterministically (bench/inputs.hpp). Each
// measurement runs once untimed as a warm-up, then repeats until it has used
// at least --min-time seconds and 5 repetitions; the median is reported.
// Every sort's output is checked against std::sort, and every search's
// results are checksummed against the query set, so a fast-but-wrong
// algorithm cannot produce a result.

#include <aao/search.hpp>
#include <aao/sort.hpp>

#include "inputs.hpp"
#include "trace.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;
using bench::SearchDist;
using bench::SortDist;

constexpr std::size_t kUnlimited = std::numeric_limits<std::size_t>::max();

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

struct Config {
    bool run_sort = true;
    bool run_search = true;
    bool quick = false;
    std::string out_dir = "results";
    std::string trace_path;
    std::string filter;
    std::size_t max_n = 0;
    double min_time = 0.2;  // seconds of measurement per cell
    int min_reps = 5;
};

void usage() {
    std::puts(
        "usage: aao_bench [sort|search|all] [options]\n"
        "\n"
        "  --quick          small sizes and short measurements (a smoke test, ~10 s)\n"
        "  --out DIR        where to write sort.csv, search.csv, meta.json (default: results)\n"
        "  --max-n N        largest input size to run\n"
        "  --min-time SEC   measurement time per cell (default: 0.2)\n"
        "  --filter TEXT    only run algorithms whose name contains TEXT\n"
        "  --trace FILE     also write a Chrome trace (open in ui.perfetto.dev)");
}

std::optional<Config> parse_args(int argc, char** argv) {
    Config c;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        auto value = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", argv[i]);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "sort") c.run_search = false;
        else if (a == "search") c.run_sort = false;
        else if (a == "all") c.run_sort = c.run_search = true;
        else if (a == "--quick") c.quick = true;
        else if (a == "--out") c.out_dir = value();
        else if (a == "--trace") c.trace_path = value();
        else if (a == "--filter") c.filter = value();
        else if (a == "--max-n") c.max_n = std::strtoull(value(), nullptr, 10);
        else if (a == "--min-time") c.min_time = std::strtod(value(), nullptr);
        else if (a == "-h" || a == "--help") return std::nullopt;
        else {
            std::fprintf(stderr, "unknown argument: %s\n\n", argv[i]);
            return std::nullopt;
        }
    }
    if (c.quick) {
        c.min_time = std::min(c.min_time, 0.01);
        c.min_reps = 1;
    }
    return c;
}

std::vector<std::size_t> geometric_sizes(int lo_log2, int hi_log2, int step, std::size_t max_n) {
    std::vector<std::size_t> sizes;
    for (int e = lo_log2; e <= hi_log2; e += step) {
        const std::size_t n = std::size_t{1} << e;
        if (max_n == 0 || n <= max_n) sizes.push_back(n);
    }
    return sizes;
}

// ---------------------------------------------------------------------------
// Measurement
// ---------------------------------------------------------------------------

struct Timing {
    std::size_t reps = 0;
    double median_ns = 0;
    double min_ns = 0;
};

template <class Prepare, class Body>
Timing measure(const Config& cfg, Prepare&& prepare, Body&& body, bench::TraceWriter* trace,
               std::string_view label, std::string_view category, int track) {
    prepare();
    body();  // warm-up: caches, branch predictors, page faults

    std::vector<double> samples;
    double total_s = 0;
    const auto begin = Clock::now();
    while ((total_s < cfg.min_time || samples.size() < static_cast<std::size_t>(cfg.min_reps)) &&
           samples.size() < 10000) {
        prepare();
        const auto t0 = Clock::now();
        body();
        const auto t1 = Clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        samples.push_back(ns);
        total_s += ns * 1e-9;
    }
    std::sort(samples.begin(), samples.end());
    Timing t;
    t.reps = samples.size();
    t.median_ns = samples[samples.size() / 2];
    t.min_ns = samples.front();
    if (trace) {
        char args[96];
        std::snprintf(args, sizeof args, "{\"reps\":%zu,\"median_ns\":%.0f}", t.reps, t.median_ns);
        trace->complete(label, category, track, begin, Clock::now(), args);
    }
    return t;
}

std::string fmt_size(std::size_t n) {
    if (n >= (1u << 20) && n % (1u << 20) == 0) return std::to_string(n >> 20) + "M";
    if (n >= (1u << 10) && n % (1u << 10) == 0) return std::to_string(n >> 10) + "K";
    return std::to_string(n);
}

bool selected(const Config& cfg, std::string_view name) {
    return cfg.filter.empty() || name.find(cfg.filter) != std::string_view::npos;
}

// ---------------------------------------------------------------------------
// Sorting
// ---------------------------------------------------------------------------

struct CountingLess {
    std::uint64_t* count;
    bool operator()(int a, int b) const {
        ++*count;
        return a < b;
    }
};

struct SortAlgo {
    const char* name;
    void (*sort)(std::vector<int>&);
    std::uint64_t (*comparisons)(std::vector<int>&);  // nullptr: not comparison-based
    std::size_t (*cap)(SortDist);                     // largest n worth timing
};

struct Variant {
    std::vector<int> input;
    std::vector<int> expected;
};

std::size_t no_cap(SortDist) { return kUnlimited; }
// O(n^2) except on sorted input.
std::size_t insertion_cap(SortDist d) { return d == SortDist::sorted ? kUnlimited : 1u << 15; }
// O(n^2) on everything but random input, with O(n) recursion depth.
std::size_t textbook_quick_cap(SortDist d) { return d == SortDist::random ? kUnlimited : 1u << 15; }

#define AAO_COMPARISON_SORT(label, fn, cap)                                \
    SortAlgo {                                                             \
        label, [](std::vector<int>& v) { fn(v.begin(), v.end()); },        \
            [](std::vector<int>& v) {                                      \
                std::uint64_t c = 0;                                       \
                fn(v.begin(), v.end(), CountingLess{&c});                  \
                return c;                                                  \
            },                                                             \
            cap                                                            \
    }

const SortAlgo kSortAlgos[] = {
    // References first, so every other row can be compared against std::sort.
    AAO_COMPARISON_SORT("std::sort", std::sort, no_cap),
    AAO_COMPARISON_SORT("std::stable_sort", std::stable_sort, no_cap),
    AAO_COMPARISON_SORT("insertion_sort", aao::insertion_sort, insertion_cap),
    AAO_COMPARISON_SORT("textbook::merge_sort", aao::textbook::merge_sort, no_cap),
    AAO_COMPARISON_SORT("merge_sort", aao::merge_sort, no_cap),
    AAO_COMPARISON_SORT("textbook::quick_sort", aao::textbook::quick_sort, textbook_quick_cap),
    AAO_COMPARISON_SORT("quick_sort", aao::quick_sort, no_cap),
    AAO_COMPARISON_SORT("intro_sort", aao::intro_sort, no_cap),
    AAO_COMPARISON_SORT("heap_sort", aao::heap_sort, no_cap),
    SortAlgo{"radix_sort", [](std::vector<int>& v) { aao::radix_sort(v.begin(), v.end()); }, nullptr,
             no_cap},
};

// Generating the adversary costs a quadratic quicksort, so it stays small.
constexpr std::size_t kAdversarialMaxN = 1u << 15;

bool run_sort_suite(const Config& cfg, bench::TraceWriter* trace) {
    const auto sizes = cfg.quick ? geometric_sizes(10, 16, 2, cfg.max_n)
                                 : geometric_sizes(10, 20, 1, cfg.max_n);
    if (sizes.empty()) return true;

    const auto path = std::filesystem::path(cfg.out_dir) / "sort.csv";
    std::ofstream csv(path);
    csv << "algorithm,distribution,n,reps,median_ns,min_ns,ns_per_element,comparisons\n";

    std::printf("\nSORTING  (ns per element, median; lower is better)\n");
    for (const SortDist dist : bench::kSortDists) {
        // Inputs and their correctly sorted versions, shared by every algorithm.
        //
        // Small inputs get up to 16 different variants, cycled between
        // repetitions. Sorting one identical array thousands of times lets a
        // modern branch predictor memorize its comparison outcomes: on the
        // machine behind results/, std::sort on the same 1K array measured
        // 2.3 ns/element versus 22.7 on fresh arrays.
        std::map<std::size_t, std::vector<Variant>> inputs;
        for (const auto n : sizes) {
            if (dist == SortDist::adversarial && n > kAdversarialMaxN) continue;
            const bool fixed = dist == SortDist::sorted || dist == SortDist::reversed ||
                               dist == SortDist::adversarial;  // identical for every seed
            const std::size_t count = fixed ? 1 : std::clamp<std::size_t>((1u << 20) / n, 1, 16);
            for (std::size_t seed = 0; seed < count; ++seed) {
                Variant v{bench::make_sort_input(dist, n, 42 + seed), {}};
                v.expected = v.input;
                std::sort(v.expected.begin(), v.expected.end());
                inputs[n].push_back(std::move(v));
            }
        }
        std::vector<std::size_t> shown;
        for (const auto& entry : inputs) shown.push_back(entry.first);
        if (shown.size() > 3) shown = {shown.front(), shown[shown.size() / 2], shown.back()};

        std::printf("\n  %-22s", std::string(bench::name(dist)).c_str());
        for (const auto n : shown) std::printf("%10s", ("n=" + fmt_size(n)).c_str());
        std::printf("   time vs std::sort, n=%s\n", fmt_size(shown.back()).c_str());

        std::map<std::string, std::map<std::size_t, double>> per_elem;
        for (const SortAlgo& algo : kSortAlgos) {
            if (!selected(cfg, algo.name)) continue;
            for (const auto& [n, variants] : inputs) {
                if (n > algo.cap(dist)) continue;
                std::vector<int> work;
                std::size_t next = 0, used = 0;
                const std::string label = std::string(algo.name) + " n=" + std::to_string(n);
                const Timing t = measure(
                    cfg,
                    [&] {
                        used = next++ % variants.size();
                        work = variants[used].input;
                    },
                    [&] { algo.sort(work); }, trace, label, bench::name(dist), 1);
                if (work != variants[used].expected) {
                    std::fprintf(stderr, "\nERROR: %s produced wrong output (%s, n=%zu)\n", algo.name,
                                 std::string(bench::name(dist)).c_str(), n);
                    return false;
                }
                std::uint64_t comparisons = 0;
                if (algo.comparisons) {
                    work = variants.front().input;
                    comparisons = algo.comparisons(work);
                }
                const double ns_elem = t.median_ns / static_cast<double>(n);
                per_elem[algo.name][n] = ns_elem;
                csv << algo.name << ',' << bench::name(dist) << ',' << n << ',' << t.reps << ','
                    << static_cast<std::uint64_t>(t.median_ns) << ',' << static_cast<std::uint64_t>(t.min_ns)
                    << ',' << ns_elem << ',';
                if (algo.comparisons) csv << comparisons;
                csv << '\n';
                csv.flush();
            }

            const auto& row = per_elem[algo.name];
            std::printf("  %-22s", algo.name);
            for (const auto n : shown) {
                const auto it = row.find(n);
                if (it == row.end()) std::printf("%10s", "-");
                else std::printf("%10.2f", it->second);
            }
            const auto& ref = per_elem["std::sort"];
            if (!row.empty() && ref.count(shown.back()) && row.rbegin()->first == shown.back()) {
                std::printf("   %6.2fx", row.rbegin()->second / ref.at(row.rbegin()->first));
            }
            std::printf("\n");
            std::fflush(stdout);
        }
    }
    std::printf("\nwrote %s\n", path.string().c_str());
    return true;
}

// ---------------------------------------------------------------------------
// Searching
// ---------------------------------------------------------------------------

struct SearchData {
    const std::vector<int>& sorted;
    const aao::eytzinger_index<int>& eytz;
};

struct SearchAlgo {
    const char* name;
    // Looks up every query; returns the sum of the elements found.
    std::uint64_t (*run)(const SearchData&, const std::vector<int>& queries);
    std::size_t (*cap)(SearchDist);
};

std::size_t search_no_cap(SearchDist) { return kUnlimited; }
// Pure interpolation search degrades towards O(n) on the skewed keys
// (~20 us per lookup at n=64K); larger sizes would take minutes per cell.
std::size_t interpolation_cap(SearchDist d) { return d == SearchDist::uniform ? kUnlimited : 1u << 16; }

template <class Find>
std::uint64_t sum_found(const std::vector<int>& queries, Find find) {
    std::uint64_t sum = 0;
    for (const int q : queries) sum += static_cast<std::uint32_t>(find(q));
    return sum;
}

#define AAO_RANGE_SEARCH(label, expr, cap)                                                      \
    SearchAlgo {                                                                                \
        label,                                                                                  \
            [](const SearchData& d, const std::vector<int>& qs) {                               \
                const int* first = d.sorted.data();                                             \
                const int* last = first + d.sorted.size();                                      \
                return sum_found(qs, [&](int key) {                                             \
                    const int* it = expr;                                                       \
                    return it == last ? 0 : *it;                                                \
                });                                                                             \
            },                                                                                  \
            cap                                                                                 \
    }

const SearchAlgo kSearchAlgos[] = {
    AAO_RANGE_SEARCH("textbook::binary_search", aao::textbook::binary_search(first, last, key),
                     search_no_cap),
    AAO_RANGE_SEARCH("std::lower_bound", std::lower_bound(first, last, key), search_no_cap),
    AAO_RANGE_SEARCH("branchless_lower_bound", aao::branchless_lower_bound(first, last, key),
                     search_no_cap),
    AAO_RANGE_SEARCH("textbook::interpolation_search",
                     aao::textbook::interpolation_search(first, last, key), interpolation_cap),
    AAO_RANGE_SEARCH("interpolation_search", aao::interpolation_search(first, last, key),
                     search_no_cap),
    SearchAlgo{"eytzinger_index",
               [](const SearchData& d, const std::vector<int>& qs) {
                   return sum_found(qs, [&](int key) {
                       const int* it = d.eytz.lower_bound(key);
                       return it ? *it : 0;
                   });
               },
               search_no_cap},
};

volatile std::uint64_t g_sink;  // keeps search results observable

bool run_search_suite(const Config& cfg, bench::TraceWriter* trace) {
    const auto sizes = cfg.quick ? geometric_sizes(10, 20, 2, cfg.max_n)
                                 : geometric_sizes(10, 26, 1, cfg.max_n);
    if (sizes.empty()) return true;
    // Enough queries that the branch predictor cannot learn their sequence.
    const std::size_t num_queries = 1u << 16;

    std::vector<std::size_t> shown = {sizes.front()};
    for (const std::size_t n : {std::size_t{1} << 16, std::size_t{1} << 20, sizes.back()})
        if (n > shown.back() && n <= sizes.back()) shown.push_back(n);

    const auto path = std::filesystem::path(cfg.out_dir) / "search.csv";
    std::ofstream csv(path);
    csv << "algorithm,distribution,n,queries,reps,median_ns_per_query,min_ns_per_query\n";

    std::printf("\nSEARCHING  (ns per lookup, median; lower is better)\n");
    for (const SearchDist dist : bench::kSearchDists) {
        std::printf("\n  %-32s", std::string(bench::name(dist)).c_str());
        for (const auto n : shown) std::printf("%10s", ("n=" + fmt_size(n)).c_str());
        std::printf("\n");

        std::map<std::string, std::map<std::size_t, double>> per_query;
        for (const auto n : sizes) {
            const auto data = bench::make_search_input(dist, n);
            const aao::eytzinger_index<int> eytz(data.begin(), data.end());
            const auto queries = bench::make_queries(data, num_queries);
            std::uint64_t expected = 0;
            for (const int q : queries) expected += static_cast<std::uint32_t>(q);
            const SearchData sd{data, eytz};

            for (const SearchAlgo& algo : kSearchAlgos) {
                if (!selected(cfg, algo.name) || n > algo.cap(dist)) continue;
                std::uint64_t got = 0;
                const std::string label = std::string(algo.name) + " n=" + std::to_string(n);
                const Timing t = measure(
                    cfg, [] {}, [&] { got = algo.run(sd, queries); }, trace, label, bench::name(dist), 2);
                g_sink = got;
                if (got != expected) {
                    std::fprintf(stderr, "\nERROR: %s returned wrong results (%s, n=%zu)\n", algo.name,
                                 std::string(bench::name(dist)).c_str(), n);
                    return false;
                }
                const double q = static_cast<double>(num_queries);
                per_query[algo.name][n] = t.median_ns / q;
                csv << algo.name << ',' << bench::name(dist) << ',' << n << ',' << num_queries << ','
                    << t.reps << ',' << t.median_ns / q << ',' << t.min_ns / q << '\n';
                csv.flush();
            }
        }
        for (const SearchAlgo& algo : kSearchAlgos) {
            if (!per_query.count(algo.name)) continue;
            const auto& row = per_query[algo.name];
            std::printf("  %-32s", algo.name);
            for (const auto n : shown) {
                const auto it = row.find(n);
                if (it == row.end()) std::printf("%10s", "-");
                else std::printf("%10.1f", it->second);
            }
            std::printf("\n");
        }
        std::fflush(stdout);
    }
    std::printf("\nwrote %s\n", path.string().c_str());
    return true;
}

// ---------------------------------------------------------------------------
// Environment
// ---------------------------------------------------------------------------

std::string cpu_name() {
    std::string name;
#if defined(__x86_64__) || defined(__i386__) || (defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86)))
    unsigned regs[12] = {};
    for (unsigned i = 0; i < 3; ++i) {
#if defined(_MSC_VER) && !defined(__clang__)
        int r[4];
        __cpuid(r, static_cast<int>(0x80000002u + i));
        std::memcpy(regs + 4 * i, r, sizeof r);
#else
        __get_cpuid(0x80000002u + i, &regs[4 * i], &regs[4 * i + 1], &regs[4 * i + 2], &regs[4 * i + 3]);
#endif
    }
    name.assign(reinterpret_cast<const char*>(regs), sizeof regs);
    name = name.c_str();  // stop at the first NUL
#elif defined(__APPLE__)
    char buf[256];
    std::size_t len = sizeof buf;
    if (sysctlbyname("machdep.cpu.brand_string", buf, &len, nullptr, 0) == 0) name = buf;
#elif defined(__linux__)
    std::ifstream info("/proc/cpuinfo");
    for (std::string line; std::getline(info, line);) {
        if (line.rfind("model name", 0) == 0 || line.rfind("Model", 0) == 0) {
            name = line.substr(line.find(':') + 2);
            break;
        }
    }
#endif
    while (!name.empty() && name.back() == ' ') name.pop_back();
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    return name.empty() ? "unknown" : name;
}

std::string compiler_name() {
#if defined(__clang__)
    return std::string("clang ") + __clang_version__;
#elif defined(__GNUC__)
    return std::string("gcc ") + __VERSION__;
#elif defined(_MSC_VER)
    return "msvc " + std::to_string(_MSC_FULL_VER);
#else
    return "unknown";
#endif
}

struct Cache {
    int level;
    std::uint64_t bytes;
};

// Data cache sizes, smallest level first. Used to mark the cache boundaries
// on the search charts; empty if the platform does not say.
std::vector<Cache> data_caches() {
    std::map<int, std::uint64_t> sizes;
#if defined(_WIN32)
    DWORD len = 0;
    GetLogicalProcessorInformation(nullptr, &len);
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
    if (!info.empty() && GetLogicalProcessorInformation(info.data(), &len)) {
        for (const auto& item : info) {
            if (item.Relationship != RelationCache) continue;
            const auto& c = item.Cache;
            if (c.Type == CacheData || c.Type == CacheUnified) sizes.emplace(c.Level, c.Size);
        }
    }
#elif defined(__APPLE__)
    const char* names[] = {"hw.l1dcachesize", "hw.l2cachesize", "hw.l3cachesize"};
    for (int level = 1; level <= 3; ++level) {
        std::uint64_t value = 0;
        std::size_t len = sizeof value;
        if (sysctlbyname(names[level - 1], &value, &len, nullptr, 0) == 0 && value > 0) sizes[level] = value;
    }
#elif defined(__linux__)
    for (int index = 0; index < 10; ++index) {
        const std::string dir = "/sys/devices/system/cpu/cpu0/cache/index" + std::to_string(index) + "/";
        std::ifstream level_file(dir + "level"), type_file(dir + "type"), size_file(dir + "size");
        int level = 0;
        std::string type, size;
        if (!(level_file >> level) || !(type_file >> type) || !(size_file >> size)) continue;
        if (type == "Instruction") continue;
        std::uint64_t bytes = std::strtoull(size.c_str(), nullptr, 10);
        if (size.back() == 'K') bytes <<= 10;
        else if (size.back() == 'M') bytes <<= 20;
        sizes.emplace(level, bytes);
    }
#endif
    std::vector<Cache> caches;
    for (const auto& [level, bytes] : sizes) caches.push_back({level, bytes});
    return caches;
}

std::string os_name() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string today() {
    const std::time_t now = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", std::gmtime(&now));
    return buf;
}

void write_meta(const Config& cfg) {
    std::ofstream meta(std::filesystem::path(cfg.out_dir) / "meta.json");
    meta << "{\n"
         << "  \"date\": \"" << today() << "\",\n"
         << "  \"cpu\": \"" << cpu_name() << "\",\n"
         << "  \"os\": \"" << os_name() << "\",\n"
         << "  \"compiler\": \"" << compiler_name() << "\",\n"
         << "  \"mode\": \"" << (cfg.quick ? "quick" : "full") << "\",\n"
         << "  \"caches\": [";
    const auto caches = data_caches();
    for (std::size_t i = 0; i < caches.size(); ++i)
        meta << (i ? ", " : "") << "{\"level\": \"L" << caches[i].level << "\", \"bytes\": " << caches[i].bytes << "}";
    meta << "]\n}\n";
}

}  // namespace

int main(int argc, char** argv) {
    const auto cfg = parse_args(argc, argv);
    if (!cfg) {
        usage();
        return 2;
    }
    std::error_code ec;
    std::filesystem::create_directories(cfg->out_dir, ec);

    std::printf("aao_bench  |  %s  |  %s  |  %s\n", cpu_name().c_str(), compiler_name().c_str(),
                os_name().c_str());
#ifndef NDEBUG
    std::printf("warning: built without NDEBUG - configure with -DCMAKE_BUILD_TYPE=Release\n");
#endif

    std::unique_ptr<bench::TraceWriter> trace;
    if (!cfg->trace_path.empty()) trace = std::make_unique<bench::TraceWriter>(cfg->trace_path);

    write_meta(*cfg);
    const auto t0 = Clock::now();
    if (cfg->run_sort && !run_sort_suite(*cfg, trace.get())) return 1;
    if (cfg->run_search && !run_search_suite(*cfg, trace.get())) return 1;
    std::printf("done in %.1f s\n", std::chrono::duration<double>(Clock::now() - t0).count());
    return 0;
}
