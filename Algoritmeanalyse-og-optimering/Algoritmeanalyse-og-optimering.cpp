/**
 * @file main.cpp
 * @brief Entry point for the application.
 *
 * This file contains the main function which initializes and runs the application.
 *
 * @author Malthe Bang Norengaard
 * @date
 */

 /**
     * @brief Runs the CLI application

     * @details

 **/

 /* ------------------------------- MAIN_CPP ----------------------------------- */

#include "main.h"
#include "raylib/raylib-5.5_win64_msvc16/include/raylib.h"

#pragma comment(lib, "raylib.lib")
#pragma comment(lib, "winmm.lib")

// Define key constants if not defined by raylib
#ifndef KEY_1
#define KEY_1           49    // Number 1 key
#endif
#ifndef KEY_2
#define KEY_2           50    // Number 2 key
#endif

BenchmarkProgress gProgress;

void ShowProgressGUI(BenchmarkProgress& progress) {
    const int windowWidth = 400;
    const int windowHeight = 300;
    const int margin = 20;

    // Calculate window position (right side of screen)
    int posX = GetScreenWidth() - windowWidth - 20;
    int posY = 70; // Below the main header

    // Draw window background with rounded corners
    Rectangle windowRect = { (float)posX, (float)posY, (float)windowWidth, (float)windowHeight };
    DrawRectangleRounded(windowRect, 0.1f, 10, Fade(WHITE, 0.95f));
    DrawRectangleRounded(windowRect, 0.1f, 10, Fade(GRAY, 0.1f));

    // Draw header
    Rectangle headerRect = { (float)posX, (float)posY, (float)windowWidth, 40.0f };
    DrawRectangleRounded(headerRect, 0.1f, 10, Fade(BLUE, 0.1f));
    DrawRectangleLines(posX, posY, windowWidth, 40, BLUE); // Simple border instead of rounded
    DrawText("Benchmark Progress", posX + margin, posY + 10, 20, DARKBLUE);

    int yPos = posY + 50;

    // Draw current operation details
    if (progress.isRunning) {
        DrawText(TextFormat("Algorithm: %s", progress.currentAlgorithm.c_str()),
                posX + margin, yPos, 16, DARKGRAY);
        yPos += 25;

        DrawText(TextFormat("Case: %s", progress.currentCase.c_str()),
                posX + margin, yPos, 16, DARKGRAY);
        yPos += 25;

        DrawText(TextFormat("Size: %d", progress.currentSize),
                posX + margin, yPos, 16, DARKGRAY);
        yPos += 25;

        DrawText(TextFormat("Progress: %d/%d", progress.currentRun, progress.totalRuns),
                posX + margin, yPos, 16, DARKGRAY);
        yPos += 25;

        // Draw progress bar
        Rectangle progressBar = {
            (float)(posX + margin),
            (float)yPos,
            (float)(windowWidth - 2 * margin),
            20
        };
        DrawRectangleRounded(progressBar, 0.5f, 10, Fade(LIGHTGRAY, 0.3f));
        DrawRectangleRounded({progressBar.x, progressBar.y, progressBar.width * progress.progress, progressBar.height},
                            0.5f, 10, Fade(GREEN, 0.7f));
        yPos += 30;
    } else {
        DrawText("No benchmark running", posX + margin, yPos, 16, GRAY);
        yPos += 30;
    }

    // Draw recent results
    DrawText("Recent Results:", posX + margin, yPos, 16, DARKGRAY);
    yPos += 25;

    for (const auto& result : progress.recentResults) {
        if (yPos + 20 > posY + windowHeight - margin) break; // Prevent overflow
        DrawText(result.c_str(), posX + margin, yPos, 14, GRAY);
        yPos += 20;
    }
}

void UpdateProgress(BenchmarkProgress& progress, const std::string& algorithm,
                   const std::string& caseType, int size, int currentRun, int totalRuns) {
    progress.currentAlgorithm = algorithm;
    progress.currentCase = caseType;
    progress.currentSize = size;
    progress.currentRun = currentRun;
    progress.totalRuns = totalRuns;
    progress.progress = static_cast<float>(currentRun) / totalRuns;
    progress.isRunning = true;
}

void AddResult(BenchmarkProgress& progress, const std::string& result) {
    progress.recentResults.insert(progress.recentResults.begin(), result);
    if (progress.recentResults.size() > 5) { // Keep last 5 results
        progress.recentResults.pop_back();
    }
}

void RunSortingBenchmarks(int num_runs, int initial_size, int size_increment) {
    gProgress.isRunning = true;
    gProgress.recentResults.clear();
    int totalOperations = num_runs * 6; // 2 algorithms * 3 cases
    int currentOperation = 0;

    Instrumentor::Get().BeginSession("Sorting Benchmarks", "results_sorting.json");

    // Pre-allocate vectors to avoid repeated allocations
    std::vector<int> merge_data;
    std::vector<int> quick_data;
    merge_data.reserve(initial_size + (num_runs - 1) * size_increment); // Reserve max size
    quick_data.reserve(initial_size + (num_runs - 1) * size_increment);

    // Initialize random number generator once
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int size = initial_size; size <= initial_size + (num_runs - 1) * size_increment; size += size_increment) {
        merge_data.resize(size);
        quick_data.resize(size);

        for (const std::string& caseType : { "Best", "Average", "Worst" }) {
            if (caseType == "Best") {
                std::iota(merge_data.begin(), merge_data.end(), 0);
            }
            else if (caseType == "Worst") {
                std::iota(merge_data.rbegin(), merge_data.rend(), 0);
            }
            else {
                std::uniform_int_distribution<> distrib(0, size * 2);
                std::generate(merge_data.begin(), merge_data.end(), [&]() { return distrib(gen); });
            }

            // Copy data for quicksort before running merge sort
            quick_data = merge_data;

            // Benchmark Merge Sort
            {
                std::string name = "Merge Sort (" + caseType + ", Size: " + std::to_string(size) + ")";
                UpdateProgress(gProgress, "Merge Sort", caseType, size, ++currentOperation, totalOperations);
                InstrumentationTimer timer(name.c_str(), caseType.c_str());
                sorting::merge_sort(merge_data);
                AddResult(gProgress, "Merge Sort - " + caseType + " - Size " + std::to_string(size));
            }

            // Benchmark Quick Sort with appropriate pivot strategy
            {
                std::string name = "Quick Sort (" + caseType + ", Size: " + std::to_string(size) + ")";
                UpdateProgress(gProgress, "Quick Sort", caseType, size, ++currentOperation, totalOperations);
                InstrumentationTimer timer(name.c_str(), caseType.c_str());
                sorting::quick_sort(quick_data, 0, quick_data.size() - 1,
                                  caseType == "Worst" ? sorting::PivotStrategy::LAST : sorting::PivotStrategy::RANDOM);
                AddResult(gProgress, "Quick Sort - " + caseType + " - Size " + std::to_string(size));
            }
        }
    }

    Instrumentor::Get().EndSession();
    SaveProfilingData("results_sorting.json");
    gProgress.isRunning = false;
}

void RunSearchingBenchmarks(int num_runs, int initial_size, int size_increment) {
    gProgress.isRunning = true;
    gProgress.recentResults.clear();
    int totalOperations = num_runs * 6; // 2 algorithms * 3 cases
    int currentOperation = 0;

    Instrumentor::Get().BeginSession("Searching Benchmarks", "results_searching.json");

    // Pre-allocate vector to avoid repeated allocations
    std::vector<int> data;
    data.reserve(initial_size + (num_runs - 1) * size_increment);

    // Initialize random number generator once
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int size = initial_size; size <= initial_size + (num_runs - 1) * size_increment; size += size_increment) {
        data.resize(size);

        for (const std::string& caseType : { "Best", "Average", "Worst" }) {
            // Generate test data with different distributions
            if (caseType == "Best") {
                std::iota(data.begin(), data.end(), 0); // Sorted ascending (best case)
            }
            else if (caseType == "Worst") {
                // For searching, we'll use a distribution that makes the search target likely to be near the end
                std::iota(data.begin(), data.end(), -size/2);
            }
            else {
                // For average case, use uniform distribution and ensure even spread
                std::uniform_int_distribution<> distrib(-size, size);
                std::generate(data.begin(), data.end(), [&]() { return distrib(gen); });
                std::sort(data.begin(), data.end());
            }

            // Select search target based on case type
            int target;
            if (caseType == "Best") {
                target = data[size / 2]; // Middle element for best case
            }
            else if (caseType == "Worst") {
                target = data.back(); // Last element for worst case
            }
            else {
                target = data[std::uniform_int_distribution<>(0, size-1)(gen)]; // Random element for average case
            }

            // Benchmark Binary Search
            {
                std::string name = "Binary Search (" + caseType + ", Size: " + std::to_string(size) + ")";
                UpdateProgress(gProgress, "Binary Search", caseType, size, ++currentOperation, totalOperations);
                InstrumentationTimer timer(name.c_str(), caseType.c_str());
                searching::binary_search(data.data(), data.size(), target);
                AddResult(gProgress, "Binary Search - " + caseType + " - Size " + std::to_string(size));
            }

            // Benchmark Interpolation Search
            {
                std::string name = "Interpolation Search (" + caseType + ", Size: " + std::to_string(size) + ")";
                UpdateProgress(gProgress, "Interpolation Search", caseType, size, ++currentOperation, totalOperations);
                InstrumentationTimer timer(name.c_str(), caseType.c_str());
                searching::interpolation_once_binary_search(data.data(), data.size(), target);
                AddResult(gProgress, "Interpolation Search - " + caseType + " - Size " + std::to_string(size));
            }
        }
    }

    Instrumentor::Get().EndSession();
    SaveProfilingData("results_searching.json");
    gProgress.isRunning = false;
}

void RunVisualizer(std::string filePath) {
    constexpr int screenWidth = 1280;
    constexpr int screenHeight = 900;

    // Configure window with anti-aliasing and VSync
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "Algorithm Performance Analyzer");

    // Set window icon and initialization code
    Color darkBlue = { 0, 82, 172, 255 };  // RGB values for dark blue
    Color skyBlue = { 135, 206, 235, 255 }; // RGB values for sky blue

    // Create a simple gradient icon
    Image icon = { 0 };
    icon.width = 32;
    icon.height = 32;
    icon.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    icon.mipmaps = 1;
    icon.data = MemAlloc(32 * 32 * 4); // 4 bytes per pixel (RGBA)

    // Fill the icon with a gradient
    unsigned char* pixels = (unsigned char*)icon.data;
    for (int y = 0; y < icon.height; y++) {
        float t = (float)y / icon.height;
        Color color = {
            (unsigned char)((1 - t) * darkBlue.r + t * skyBlue.r),
            (unsigned char)((1 - t) * darkBlue.g + t * skyBlue.g),
            (unsigned char)((1 - t) * darkBlue.b + t * skyBlue.b),
            255
        };

        for (int x = 0; x < icon.width; x++) {
            int offset = (y * icon.width + x) * 4;
            pixels[offset] = color.r;
            pixels[offset + 1] = color.g;
            pixels[offset + 2] = color.b;
            pixels[offset + 3] = color.a;
        }
    }

    SetWindowIcon(icon);

    // More detailed runVisualizer
    std::vector<std::string> resultFiles = {"results_sorting.json", "results_searching.json"};
    int currentFileIndex = 0;
    std::vector<TraceEvent> traceEvents = parseJSON(resultFiles[currentFileIndex]);

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // Handle visualization switching
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_1)) {
            currentFileIndex = 0; // Switch to sorting view
            traceEvents = parseJSON(resultFiles[currentFileIndex]);
        }
        if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_2)) {
            currentFileIndex = 1; // Switch to searching view
            traceEvents = parseJSON(resultFiles[currentFileIndex]);
        }

        // Toggle fullscreen with Alt+Enter or F11
        if ((IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))) ||
            IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
        }

        if ((IsKeyPressed(KEY_ESCAPE))) {
            break;
        }

        BeginDrawing();
        ClearBackground(GetColor(0xFAFAFAFF)); // Light gray background for better contrast

        // Draw modern header bar with gradient
        DrawRectangleGradientH(0, 0, GetScreenWidth(), 50, GetColor(0x2C3E50FF), GetColor(0x3498DBFF));
        DrawText("Algorithm Performance Analyzer", 20, 15, 20, WHITE);

        // Draw elegant controls help with icons
        const char* controls = "Controls:  [1,S] Sorting Analysis  |  [2,F] Searching Analysis  |  [F11] Fullscreen  |  [Esc] Exit";
        DrawText(controls, GetScreenWidth() - MeasureText(controls, 10) - 20, 20, 10, Fade(WHITE, 0.9f));

        // Draw current view indicator with more visibility
        const char* viewText = (currentFileIndex == 0) ? "SORTING ANALYSIS VIEW" : "SEARCHING ANALYSIS VIEW";
        int viewTextWidth = MeasureText(viewText, 20);
        DrawRectangleGradientH(GetScreenWidth()/2 - viewTextWidth/2 - 10, 10,
                              viewTextWidth + 20, 30,
                              Fade(DARKBLUE, 0.3f), Fade(DARKBLUE, 0.1f));
        DrawText(viewText, GetScreenWidth()/2 - viewTextWidth/2, 15, 20, WHITE);

        if (!traceEvents.empty()) {
            plotData(traceEvents, GetScreenWidth(), GetScreenHeight(), resultFiles[currentFileIndex]);
        } else {
            // Enhanced error message display
            DrawRectangleGradientV(0, GetScreenHeight()/2 - 50, GetScreenWidth(), 100,
                                 Fade(RED, 0.1f), Fade(RED, 0.2f));
            const char* message = "No performance data available. Please run benchmarks first.";
            int textWidth = MeasureText(message, 24);
            DrawText(message, GetScreenWidth()/2 - textWidth/2, GetScreenHeight()/2 - 12, 24, RED);
        }

        // Draw progress GUI if benchmarks are running
        ShowProgressGUI(gProgress);

        EndDrawing();
    }

    UnloadImage(icon);
    MemFree(icon.data);
    CloseWindow();
}

bool BENCHMARK = true;
bool VISUALIZE = true;
bool ENABLE_WARMUP = true;  // Enable warmup runs

// Configuration constants for benchmarking
const int WARMUP_RUNS = 3;
const int WARMUP_INITIAL_SIZE = 1000;
const int WARMUP_SIZE_INCREMENT = 1000;

const int BENCHMARK_RUNS = 15;       // Increased from 20 for more data points
const int INITIAL_SIZE = 1000;       // Reduced initial size for more granular data
const int SIZE_INCREMENT = 2000;     // Smaller increments for more data points

int main() {
    // Allow switching between sorting and searching results
    std::vector<std::string> resultFiles = {"results_sorting.json", "results_searching.json"};
    int currentFileIndex = 0;

    if (BENCHMARK) {
        // Run both sorting and searching benchmarks
        std::cout << "Running sorting benchmarks..." << std::endl;
        if (ENABLE_WARMUP) {
            std::cout << "Performing sorting warmup runs..." << std::endl;
            RunSortingBenchmarks(WARMUP_RUNS, WARMUP_INITIAL_SIZE, WARMUP_SIZE_INCREMENT);
            std::cout << "Warmup complete. Starting actual benchmarks..." << std::endl;
        }
        RunSortingBenchmarks(BENCHMARK_RUNS, INITIAL_SIZE, SIZE_INCREMENT);

        std::cout << "\nRunning searching benchmarks..." << std::endl;
        if (ENABLE_WARMUP) {
            std::cout << "Performing searching warmup runs..." << std::endl;
            RunSearchingBenchmarks(WARMUP_RUNS, WARMUP_INITIAL_SIZE, WARMUP_SIZE_INCREMENT);
            std::cout << "Warmup complete. Starting actual benchmarks..." << std::endl;
        }
        RunSearchingBenchmarks(BENCHMARK_RUNS, INITIAL_SIZE, SIZE_INCREMENT);
    }

    if (VISUALIZE) {
        RunVisualizer(resultFiles[currentFileIndex]);
    }

    return 0;
}
