# Contributing

Thanks for taking a look. Three kinds of contribution are especially useful.

## Benchmark results from your machine

Different processors tell different stories: a smaller L3 cache moves the bend in the search curves, and other
compilers make other choices about branches.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/aao_bench --out results-contrib
```

Open a pull request that adds the folder as `results/contrib/<cpu>-<compiler>/` (for example
`results/contrib/apple-m3-clang16/`). Please close other programs while it runs, and mention whether the machine is a
laptop on battery.

## A new rung on the ladder

An algorithm or optimization fits this repository if it can be measured against something already here. A pull
request should:

1. add the function to `include/aao/sort.hpp` or `include/aao/search.hpp`, with a comment that says what it changes
   and why it should help;
2. add it to the tests in `tests/tests.cpp` (every input shape, plus its edge cases);
3. register it in `bench/main.cpp` and include before/after numbers in the description.

If it also belongs in the visualizer, port it to `web/js/sorting.js` with the same structure as the existing
algorithms and extend `tests/sorting.test.mjs`.

## Bugs and corrections

If a number in the README or in `docs/analysis.md` does not match what you measure, or an explanation is wrong, open
an issue with what you ran and what you saw.

## Checks

Before opening a pull request:

```sh
ctest --test-dir build -C Release        # C++ tests
node --test "tests/*.test.mjs"           # JavaScript ports
node tools/plot.mjs && node tools/hero.mjs  # only if results/ or the chart code changed
```

C++ follows the style of the surrounding code: 4-space indentation, `snake_case`, and comments that explain why.
