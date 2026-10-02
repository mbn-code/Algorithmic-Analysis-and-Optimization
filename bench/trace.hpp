// SPDX-License-Identifier: MIT
//
// Minimal Chrome Trace Event writer. Open the output in https://ui.perfetto.dev
// or chrome://tracing to see every measurement on a timeline.
//
// Descends from the instrumentation profiler in v1 of this project (itself
// based on The Cherno's "Basic instrumentation profiler"), with the v1 bugs
// fixed: names are escaped, timestamps use a steady clock, and the file is
// valid JSON even if the run is interrupted between events.

#pragma once

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>

namespace bench {

class TraceWriter {
public:
    using Clock = std::chrono::steady_clock;

    explicit TraceWriter(const std::string& path) : out_(path), origin_(Clock::now()) {
        out_ << "{\"displayTimeUnit\":\"ns\",\"traceEvents\":[\n";
    }

    TraceWriter(const TraceWriter&) = delete;
    TraceWriter& operator=(const TraceWriter&) = delete;

    ~TraceWriter() { out_ << "\n]}\n"; }

    [[nodiscard]] bool ok() const { return out_.good(); }

    /// One "complete" event: a named span from `start` to `end` on track `tid`.
    void complete(std::string_view name, std::string_view category, int tid, Clock::time_point start,
                  Clock::time_point end, std::string_view args_json = "{}") {
        if (!first_) out_ << ",\n";
        first_ = false;
        out_ << "{\"name\":\"" << escape(name) << "\",\"cat\":\"" << escape(category)
             << "\",\"ph\":\"X\",\"pid\":1,\"tid\":" << tid << ",\"ts\":" << micros(start - origin_)
             << ",\"dur\":" << micros(end - start) << ",\"args\":" << args_json << "}";
        out_.flush();
    }

private:
    static std::string micros(Clock::duration d) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.3f", std::chrono::duration<double, std::micro>(d).count());
        return buf;
    }

    static std::string escape(std::string_view s) {
        std::string r;
        for (const char c : s) {
            if (c == '"' || c == '\\') r += '\\';
            if (static_cast<unsigned char>(c) >= 0x20) r += c;
        }
        return r;
    }

    std::ofstream out_;
    Clock::time_point origin_;
    bool first_ = true;
};

}  // namespace bench
