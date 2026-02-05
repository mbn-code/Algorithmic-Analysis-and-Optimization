# Algorithm Analysis and Optimization

[![GitHub Issues](https://img.shields.io/github/issues/mbn-code/Algoritmeanalyse-og-optimering)](https://github.com/mbn-code/Algoritmeanalyse-og-optimering/issues)
[![GitHub Stars](https://img.shields.io/github/stars/mbn-code/Algoritmeanalyse-og-optimering)](https://github.com/mbn-code/Algoritmeanalyse-og-optimering/stargazers)
[![GitHub Forks](https://img.shields.io/github/forks/mbn-code/Algoritmeanalyse-og-optimering)](https://github.com/mbn-code/Algoritmeanalyse-og-optimering/network/members)
[![License](https://img.shields.io/github/license/mbn-code/Algoritmeanalyse-og-optimering)](https://github.com/mbn-code/Algoritmeanalyse-og-optimering/blob/main/LICENSE)

This project focuses on algorithm optimization, analysis, and practical implementations using sorting and searching algorithms. It includes benchmarking and visualization tools to analyze the performance of different algorithms in various scenarios.

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Installation](#installation)
- [macOS-Specific-installation] (#macos-installation)
- [Usage](#usage)
  - [Running Benchmarks](#running-benchmarks)
  - [Visualizing Sorting Algorithms](#visualizing-sorting-algorithms)
  - [Viewing Profiling Data](#viewing-profiling-data)
- [Detailed Analysis](#detailed-analysis)
  - [Sorting Algorithms](#sorting-algorithms)
  - [Searching Algorithms](#searching-algorithms)
- [Results](#results)
- [Conclusion](#conclusion)
- [Project Overview](#project-overview)
- [Problem Statement](#problem-statement)
- [Questions](#questions)
- [Contributing](#contributing)
- [License](#license)

## Introduction

The aim of this project is to investigate commonly used methods for optimizing algorithms and how these methods are classified. It delves into how mathematics is used to analyze algorithms and their performance, specifically focusing on Merge Sort, Quick Sort, and Binary Search algorithms.

The project stems from the need to understand how algorithm analysis and optimization can save time and computational resources. Algorithms are fundamental in solving problems efficiently, and optimizing them is crucial in practical applications like search engines, data processing, and more.

Relevant resources for learning about algorithm optimization include:

- [GeeksforGeeks - Analysis of Algorithms (Theta Notation)](https://www.geeksforgeeks.org/analysis-of-algorithms-big-theta-notation/)
- [Wikiwand - Time Complexity](https://www.wikiwand.com/en/articles/Time%20complexity)
- [Khan Academy - Analysis of Quick Sort](https://www.khanacademy.org/computing/computer-science/algorithms/quick-sort/a/analysis-of-quicksort)

## Features

- **Sorting Algorithms Implementation**: C++ implementations of Merge Sort and Quick Sort with different pivot strategies.
- **Searching Algorithm Implementation**: C++ implementation of Binary Search.
- **Benchmarking Tool**: Measures the performance of sorting and searching algorithms under Best, Average, and Worst-case scenarios.
- **Visualization**: Python script using `matplotlib` to animate sorting processes and highlight code.
- **Profiling**: C++ code profiles algorithm performance and outputs the results to JSON files compatible with Chrome Tracing.

Learn more about sorting and searching algorithm concepts at:

- [GeeksforGeeks - Binary Search Algorithm](https://www.geeksforgeeks.org/binary-search/)
- [W3Schools - Quick Sort](https://www.w3schools.com/dsa/dsa_algo_quicksort.php)

## Installation

### Prerequisites

- **C++ Compiler**: Compatible with C++11 or higher (e.g., GCC, Clang, MSVC).
- **Python 3.x**: Along with the following packages:
  - `numpy`
  - `matplotlib`

To understand benchmarking better, check:

- [The Cherno - Benchmarking in C++](https://www.youtube.com/watch?v=YG4jexlSAjc)

### Setup Steps

1. **Clone the Repository**

   ```bash
   git clone https://github.com/mbn-code/Algoritmeanalyse-og-optimering.git
   ```

2. **Build the C++ Project**
   - **On macOS/Linux (using Makefile)**

     ```bash
     cd Algoritmeanalyse-og-optimering
     make
     ```

   - **On Windows (using Visual Studio)**
     - Open the solution file in Visual Studio.
     - Build the project from the **Build** menu.

3. **Install Python Dependencies**

   ```bash
   pip install numpy matplotlib
   ```

## macOS Installation

This section provides detailed instructions for setting up and compiling the project specifically on macOS.

### Prerequisites for macOS

Before building the project, ensure you have the following installed:

1. **Xcode Command Line Tools** (includes GCC/Clang compiler)

   ```bash
   xcode-select --install
   ```

2. **Homebrew** (package manager for macOS)

   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

3. **raylib** (graphics library)

   ```bash
   brew install raylib
   ```

4. **pkg-config** (required to find raylib packages)

   ```bash
   brew install pkg-config
   ```

5. **Bear** (to generate compile_commands.json)

   ```bash
   brew install bear
   ```

6. **Python 3.x** with required packages
   ```bash
   pip install numpy matplotlib
   ```

### Compiling on macOS

Navigate to the project directory and run the following compilation command:

```bash
cd /Users/mbn/Documents/GitHub/Algorithmic-Analysis-and-Optimization/Algoritmeanalyse-og-optimering && \
g++ -std=c++17 -o Algoritmeanalyse-og-optimering *.cpp \
  -I. \
  $(pkg-config --cflags --libs raylib) \
  -lm -lpthread; ./Algoritmeanalyse-og-optimering
```

**Explanation**:

- `-std=c++17`: Uses the C++17 standard
- `-o Algoritmeanalyse-og-optimering`: Names the output executable
- `*.cpp`: Compiles all C++ source files
- `-I.`: Includes the current directory for headers
- `$(pkg-config --cflags --libs raylib)`: Automatically includes raylib compilation flags and libraries
- `-lm`: Links the math library
- `-lpthread`: Links the pthread library for threading support

### Generating compile_commands.json on macOS

To generate the `compile_commands.json` file needed for code analysis tools and IDE language servers, use the `bear` command:

```bash
cd /Users/mbn/Documents/GitHub/Algorithmic-Analysis-and-Optimization/Algoritmeanalyse-og-optimering && \
bear -- g++ -std=c++17 -o Algoritmeanalyse-og-optimering *.cpp \
  -I. \
  $(pkg-config --cflags --libs raylib) \
  -lm -lpthread
```

This command wraps your compilation command with `bear --` and generates a `compile_commands.json` file in the current directory. This file is essential for:

- Code intelligence and autocomplete in IDEs (VS Code, CLion, etc.)
- Static analysis tools
- Language server protocol (LSP) support

### Running the Application

After successful compilation, the executable will automatically run. If you only want to compile without running:

```bash
cd /Users/mbn/Documents/GitHub/Algorithmic-Analysis-and-Optimization/Algoritmeanalyse-og-optimering && \
g++ -std=c++17 -o Algoritmeanalyse-og-optimering *.cpp \
  -I. \
  $(pkg-config --cflags --libs raylib) \
  -lm -lpthread
```

Then run the executable separately:

```bash
./Algoritmeanalyse-og-optimering
```

### Troubleshooting

- **"raylib not found"**: Ensure raylib is installed (`brew install raylib`) and pkg-config can find it (`pkg-config --cflags --libs raylib`).
- **Compilation errors**: Verify you have Xcode Command Line Tools installed (`xcode-select --install`).
- **Permission denied**: Make sure the executable has execute permissions (`chmod +x Algoritmeanalyse-og-optimering`).

## Detailed Analysis

### Sorting Algorithms

- **Quick Sort**: An efficient, in-place sorting algorithm using the divide-and-conquer approach. It selects a 'pivot' element and partitions the array around the pivot. While it has an average-case time complexity of O(n log n), the worst-case performance is O(n²).

  For further reading, refer to:
  - [CS Dojo - Time Complexity Introduction](https://www.youtube.com/watch?v=D6xkbGLQesk)
  - [GeeksforGeeks - Quick Sort Complexity](https://www.geeksforgeeks.org/time-and-space-complexity-analysis-of-quick-sort/)

- **Merge Sort**: A stable, comparison-based sorting algorithm that consistently performs at O(n log n) time complexity for all cases. It divides the array into halves, sorts them recursively, and then merges the sorted halves.

  Additional resource:
  - [Omegapy - Merge Sort Analysis](https://www.alexomegapy.com/post/merge-sort-divide-and-conquer-for-large-datasets)

### Searching Algorithms

- **Binary Search**: An efficient algorithm for finding an item in a sorted array with a time complexity of O(log n).

  Learn more about Binary Search:
  - [GeeksforGeeks - Binary Search](https://www.geeksforgeeks.org/binary-search/)

## Results

Benchmarking results and profiling data are available for both sorting and searching algorithms.

- **Sorting Results**: [View Sorting Results](Algoritmeanalyse-og-optimering/results_sorting.json)
- **Searching Results**: [View Searching Results](Algoritmeanalyse-og-optimering/results_searching.json)

For detailed benchmarks, check:

- [HPC Wiki - Micro Benchmarking](https://hpc-wiki.info/hpc/Micro_benchmarking)

## Conclusion

In this project, we explored algorithm analysis and optimization, focusing on sorting and searching algorithms. By implementing and benchmarking Quick Sort, Merge Sort, and Binary Search, we gained insights into their performance characteristics in various scenarios. Visualizing the algorithms helped in understanding their behavior and the importance of choosing the right algorithm for a specific problem to optimize time and space complexity.
