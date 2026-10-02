// SPDX-License-Identifier: MIT
//
// aao_viewer - a native window for the benchmark results, drawn with raylib.
//
//   aao_viewer [results-dir] [--screenshot FILE]     (default dir: results)
//
//   Tab           switch between sorting and searching
//   Left / Right  previous / next input distribution
//   Click legend  hide or show an algorithm
//   P             save a screenshot
//
// The successor of v1's "Algorithm Performance Analyzer", reading the CSV
// files written by aao_bench instead of Chrome trace JSON.

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Row {
    std::string algorithm;
    std::string distribution;
    double n = 0;
    double value = 0;
};

struct Suite {
    std::string title;
    std::string unit;
    std::vector<Row> rows;
    std::vector<std::string> distributions;
    std::vector<std::string> algorithms;
};

// Same palette as the web page (web/style.css): hue = algorithm family,
// dashed = the textbook version, paper white = the standard library.
const Color kBackground{15, 44, 76, 255};
const Color kPanel{11, 35, 64, 255};
const Color kInk{233, 241, 248, 255};
const Color kMuted{169, 191, 212, 255};
const Color kRule{233, 241, 248, 40};

struct Style {
    Color color;
    bool dashed;
};

Style style_for(const std::string& name) {
    const Color orange{217, 89, 38, 255}, aqua{25, 158, 112, 255}, yellow{201, 133, 0, 255},
        magenta{213, 81, 129, 255}, violet{144, 133, 233, 255};
    const bool textbook = name.rfind("textbook::", 0) == 0;
    if (name.find("merge") != std::string::npos || name.find("binary") != std::string::npos ||
        name == "branchless_lower_bound")
        return {orange, textbook};
    if (name.find("quick") != std::string::npos || name == "intro_sort" ||
        name.find("interpolation") != std::string::npos)
        return {aqua, textbook || name == "intro_sort"};
    if (name == "radix_sort" || name == "eytzinger_index") return {yellow, false};
    if (name == "insertion_sort") return {magenta, false};
    if (name == "heap_sort") return {violet, false};
    return {kInk, name == "std::stable_sort"};
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> cells;
    std::stringstream ss(line);
    for (std::string cell; std::getline(ss, cell, ',');) cells.push_back(cell);
    if (!line.empty() && line.back() == ',') cells.emplace_back();  // getline drops a trailing empty field
    return cells;
}

Suite load(const std::string& path, const std::string& metric, std::string title, std::string unit) {
    Suite suite{std::move(title), std::move(unit), {}, {}, {}};
    std::ifstream in(path);
    std::string line;
    if (!std::getline(in, line)) return suite;
    const auto head = split(line);
    auto column = [&](const std::string& name) {
        return static_cast<std::size_t>(std::find(head.begin(), head.end(), name) - head.begin());
    };
    const std::size_t a = column("algorithm"), d = column("distribution"), n = column("n"), v = column(metric);
    while (std::getline(in, line)) {
        const auto cells = split(line);
        if (cells.size() < head.size()) continue;
        Row r{cells[a], cells[d], std::atof(cells[n].c_str()), std::atof(cells[v].c_str())};
        if (r.n <= 0 || r.value <= 0) continue;
        if (std::find(suite.distributions.begin(), suite.distributions.end(), r.distribution) ==
            suite.distributions.end())
            suite.distributions.push_back(r.distribution);
        if (std::find(suite.algorithms.begin(), suite.algorithms.end(), r.algorithm) == suite.algorithms.end())
            suite.algorithms.push_back(r.algorithm);
        suite.rows.push_back(std::move(r));
    }
    return suite;
}

Font load_font() {
    const char* candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    };
    for (const char* path : candidates) {
        if (FileExists(path)) {
            Font font = LoadFontEx(path, 64, nullptr, 0);
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            return font;
        }
    }
    return GetFontDefault();
}

Font g_font;

void text(const std::string& s, float x, float y, float size, Color color) {
    DrawTextEx(g_font, s.c_str(), {x, y}, size, 0, color);
}

float text_width(const std::string& s, float size) { return MeasureTextEx(g_font, s.c_str(), size, 0).x; }

std::string fmt_n(double n) {
    char buf[32];
    if (n >= 1 << 20) std::snprintf(buf, sizeof buf, "%gM", n / (1 << 20));
    else if (n >= 1024) std::snprintf(buf, sizeof buf, "%gK", n / 1024);
    else std::snprintf(buf, sizeof buf, "%g", n);
    return buf;
}

std::string fmt_v(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, v >= 100 ? "%.0f" : v >= 10 ? "%.1f" : "%.2f", v);
    return buf;
}

void dashed_line(Vector2 a, Vector2 b, Color color, bool dashed) {
    if (!dashed) {
        DrawLineEx(a, b, 2.0f, color);
        return;
    }
    const float len = std::hypot(b.x - a.x, b.y - a.y);
    for (float t = 0; t < len; t += 10) {
        const float u = t / len, w = std::min(t + 6, len) / len;
        DrawLineEx({a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u}, {a.x + (b.x - a.x) * w, a.y + (b.y - a.y) * w},
                   2.0f, color);
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string dir = "results";
    std::string snapshot;  // --screenshot: save the first frames to a file and exit
    for (int a = 1; a < argc; ++a) {
        const std::string arg = argv[a];
        if (arg == "--screenshot" && a + 1 < argc) snapshot = argv[++a];
        else dir = arg;
    }
    std::vector<Suite> suites = {
        load(dir + "/sort.csv", "ns_per_element", "Sorting 32-bit integers", "ns per element"),
        load(dir + "/search.csv", "median_ns_per_query", "Looking up random keys", "ns per lookup"),
    };

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "aao viewer");
    SetTargetFPS(60);
    g_font = load_font();

    std::size_t suite_index = 0, dist_index = 0;
    std::set<std::string> hidden;
    int frames = 0;

    while (!WindowShouldClose()) {
        if (!snapshot.empty() && ++frames == 4) {
            Image shot = LoadImageFromScreen();
            ExportImage(shot, snapshot.c_str());
            UnloadImage(shot);
            break;
        }
        Suite& suite = suites[suite_index];
        if (IsKeyPressed(KEY_TAB)) {
            suite_index = (suite_index + 1) % suites.size();
            dist_index = 0;
            continue;
        }
        if (!suite.distributions.empty()) {
            const std::size_t count = suite.distributions.size();
            if (IsKeyPressed(KEY_RIGHT)) dist_index = (dist_index + 1) % count;
            if (IsKeyPressed(KEY_LEFT)) dist_index = (dist_index + count - 1) % count;
        }
        if (IsKeyPressed(KEY_P)) TakeScreenshot("aao_viewer.png");

        const float W = static_cast<float>(GetScreenWidth()), H = static_cast<float>(GetScreenHeight());
        BeginDrawing();
        ClearBackground(kBackground);

        if (suite.rows.empty()) {
            text("No results found in " + dir + "/. Run aao_bench first, or pass the results folder.", 40, H / 2,
                 24, kInk);
            EndDrawing();
            continue;
        }

        const std::string& dist = suite.distributions[dist_index];
        text(suite.title, 40, 28, 30, kInk);
        text(dist + "   (" + std::to_string(dist_index + 1) + "/" + std::to_string(suite.distributions.size()) +
                 ")   Left/Right: input   Tab: " + (suite_index ? "sorting" : "searching") + "   P: screenshot",
             40, 66, 18, kMuted);

        // Legend: click to toggle.
        float lx = 40, ly = 100;
        const Vector2 mouse = GetMousePosition();
        for (const auto& name : suite.algorithms) {
            const float w = text_width(name, 17) + 44;
            if (lx + w > W - 40) {
                lx = 40;
                ly += 28;
            }
            const Rectangle box{lx, ly, w, 24};
            const bool off = hidden.count(name) > 0;
            if (CheckCollisionPointRec(mouse, box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (off) hidden.erase(name);
                else hidden.insert(name);
            }
            const Style s = style_for(name);
            dashed_line({lx + 6, ly + 12}, {lx + 30, ly + 12}, off ? Fade(s.color, 0.3f) : s.color, s.dashed);
            text(name, lx + 36, ly + 3, 17, off ? kMuted : kInk);
            lx += w + 10;
        }

        // Plot area, log-log.
        const Rectangle plot{110, ly + 50, W - 150, H - ly - 130};
        DrawRectangleRec({plot.x - 20, plot.y - 20, plot.width + 40, plot.height + 70}, kPanel);
        std::vector<const Row*> rows;
        for (const auto& r : suite.rows)
            if (r.distribution == dist && !hidden.count(r.algorithm)) rows.push_back(&r);
        if (rows.empty()) {
            EndDrawing();
            continue;
        }
        double nlo = 1e300, nhi = 0, vlo = 1e300, vhi = 0;
        for (const Row* r : rows) {
            nlo = std::min(nlo, r->n), nhi = std::max(nhi, r->n);
            vlo = std::min(vlo, r->value), vhi = std::max(vhi, r->value);
        }
        const double ylo = std::floor(std::log10(vlo * 0.8)), yhi = std::log10(vhi * 1.3);
        auto px = [&](double n) {
            return plot.x + static_cast<float>((std::log2(n) - std::log2(nlo)) / std::max(1.0, std::log2(nhi / nlo))) *
                                plot.width;
        };
        auto py = [&](double v) {
            return plot.y + static_cast<float>(1 - (std::log10(v) - ylo) / (yhi - ylo)) * plot.height;
        };

        for (double e = ylo; e <= std::ceil(yhi); ++e) {
            for (double k : {1.0, 2.0, 5.0}) {
                const double v = k * std::pow(10, e);
                if (std::log10(v) > yhi) continue;
                DrawLineEx({plot.x, py(v)}, {plot.x + plot.width, py(v)}, 1, kRule);
                char label_buf[16];
                std::snprintf(label_buf, sizeof label_buf, "%g", v);
                const std::string label = label_buf;
                text(label, plot.x - 14 - text_width(label, 16), py(v) - 9, 16, kMuted);
            }
        }
        std::set<double> sizes;
        for (const Row* r : rows) sizes.insert(r->n);
        int i = 0;
        for (double n : sizes) {
            if (i++ % 2 == 0 || n == nhi) {
                const std::string label = fmt_n(n);
                text(label, px(n) - text_width(label, 16) / 2, plot.y + plot.height + 12, 16, kMuted);
            }
        }
        DrawLineEx({plot.x, plot.y + plot.height}, {plot.x + plot.width, plot.y + plot.height}, 1, kMuted);
        text(suite.unit + " (log scale)", plot.x, plot.y + plot.height + 34, 15, kMuted);

        std::map<std::string, std::vector<const Row*>> series;
        for (const Row* r : rows) series[r->algorithm].push_back(r);
        for (auto& [name, points] : series) {
            std::sort(points.begin(), points.end(), [](const Row* a, const Row* b) { return a->n < b->n; });
            const Style s = style_for(name);
            for (std::size_t k = 1; k < points.size(); ++k)
                dashed_line({px(points[k - 1]->n), py(points[k - 1]->value)}, {px(points[k]->n), py(points[k]->value)},
                            s.color, s.dashed);
        }

        // Hover: crosshair at the nearest size and a tooltip with every value.
        if (CheckCollisionPointRec(mouse, plot)) {
            double best = *sizes.begin();
            for (double n : sizes)
                if (std::fabs(px(n) - mouse.x) < std::fabs(px(best) - mouse.x)) best = n;
            DrawLineEx({px(best), plot.y}, {px(best), plot.y + plot.height}, 1, Fade(kInk, 0.5f));
            std::vector<const Row*> at;
            for (const Row* r : rows)
                if (r->n == best) at.push_back(r);
            std::sort(at.begin(), at.end(), [](const Row* a, const Row* b) { return a->value < b->value; });
            const float tw = 330, th = 34 + 22 * static_cast<float>(at.size());
            float tx = px(best) + 16;
            if (tx + tw > W - 10) tx = px(best) - 16 - tw;
            DrawRectangleRec({tx, plot.y, tw, th}, kBackground);
            DrawRectangleLinesEx({tx, plot.y, tw, th}, 1, kRule);
            text("n = " + fmt_n(best), tx + 12, plot.y + 8, 17, kInk);
            float row_y = plot.y + 32;
            for (const Row* r : at) {
                DrawRectangleRec({tx + 12, row_y + 8, 12, 3}, style_for(r->algorithm).color);
                text(r->algorithm, tx + 32, row_y, 16, kInk);
                const std::string v = fmt_v(r->value);
                text(v, tx + tw - 12 - text_width(v, 16), row_y, 16, kInk);
                row_y += 22;
            }
        }
        EndDrawing();
    }

    if (g_font.texture.id != GetFontDefault().texture.id) UnloadFont(g_font);
    CloseWindow();
    return 0;
}
