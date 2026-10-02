# Analysis: what the math predicts, and what the machine does

This project started as a question from a Danish upper-secondary study project:
_how well does the mathematical analysis of an algorithm predict how it behaves
in practice?_ This document answers it twice. First by counting comparisons,
where theory turns out to be almost exact. Then by measuring time, where it
explains the shape of the curves but misses factors of 10 or more.

All measured numbers come from [`results/`](../results), produced by
`aao_bench` on the machine described in [`results/meta.json`](../results/meta.json).
Comparison counts do not depend on the machine; times do.

## Notation

For functions f and g on the positive integers:

- f(n) = O(g(n)) if there are constants c > 0 and n₀ such that f(n) ≤ c·g(n) for all n ≥ n₀.
- f(n) = Ω(g(n)) if g(n) = O(f(n)).
- f(n) = Θ(g(n)) if both hold.

Big-O hides two things this project cares about: the constant c, and the cost of
one "step". The rest of this document is about both.

## Part 1: counting comparisons

Every comparison sort in `include/aao/sort.hpp` accepts a comparator, so the
benchmark can count comparisons exactly by passing one that increments a
counter. These are the counts in the `comparisons` column of `sort.csv`.

### Insertion sort

Inserting element i into the sorted prefix costs one comparison per element it
moves past, plus one to stop. So the comparison count is n − 1 plus the number
of inversions, the pairs that are out of order:

| Input    | Inversions          | Comparisons                       | Measured at n = 32,768 |
| -------- | ------------------- | --------------------------------- | ---------------------- |
| sorted   | 0                   | n − 1 = 32,767                    | 32,767                 |
| reversed | n(n−1)/2            | n(n−1)/2 = 536,854,528            | 536,854,528            |
| random   | n(n−1)/4 on average | ≈ n(n−1)/4 + n − Hₙ = 268,460,021 | 268,696,353 (+0.09%)   |

Θ(n²) on average and in the worst case, Θ(n) when the input is already sorted.
That last line is why every fast sort in this repository finishes small ranges
with insertion sort.

### Merge sort

The recurrence for the number of comparisons is

    T(1) = 0,   T(n) = T(⌊n/2⌋) + T(⌈n/2⌉) + (comparisons to merge n elements)

Merging two runs of total length n takes at most n − 1 comparisons, and at
least half that (when one run is exhausted before the other is touched). The
recursion tree has ⌈log₂ n⌉ levels, each merging n elements in total, so
T(n) = Θ(n log n), which the master theorem confirms (a = 2, b = 2, f(n) = Θ(n)).

The average is known precisely: about n log₂ n − 1.26n.

| Input    | Theory at n = 2²⁰                 | Measured (`textbook::merge_sort`) |
| -------- | --------------------------------- | --------------------------------- |
| random   | n log₂ n − 1.2645n ≈ 19,645,596   | 19,645,004                        |
| sorted   | exactly (n/2) log₂ n = 10,485,760 | 10,485,760                        |
| reversed | exactly (n/2) log₂ n = 10,485,760 | 10,485,760                        |

The tuned `merge_sort` checks whether two runs are already in order before
merging them. On sorted input that check always succeeds, the merges never
happen, and the count drops to n − 1 = 1,048,575: linear.

### The lower bound, and how to get under it

A comparison sort has to tell apart all n! orderings of its input, and each
comparison has two outcomes, so some input needs at least log₂(n!) comparisons.
By Stirling's formula, log₂(n!) = n log₂ n − 1.443n + O(log n). At n = 2²⁰:

    log₂(n!) ≈ 19,458,756

`textbook::merge_sort` used 19,645,004 comparisons on random input: within
**1.0%** of the minimum any comparison sort can achieve on average.

So how is `radix_sort` 14x faster than `std::sort`? It never compares two
elements. It reads each key one byte at a time and uses the byte as an array
index, which takes O(n · w) for w-byte keys. The lower bound only applies to
algorithms that learn about the order through comparisons.

### Quicksort

With pivot p splitting n elements into parts of size k and n − k − 1, partitioning
costs about n comparisons, so

    worst case:  T(n) = T(n − 1) + n − 1   ⇒   T(n) = n(n−1)/2
    best case:   T(n) = 2T(n/2) + n − 1    ⇒   T(n) = n log₂ n + O(n)

The textbook version picks the last element as pivot. On sorted input that is
always the maximum, so every partition peels off a single element: the worst
case, exactly. At n = 32,768 the benchmark counted 536,854,528 comparisons,
which is n(n−1)/2 to the last digit.

On random input every element is equally likely to be the pivot, and the
expected count solves to

    C(n) = 2(n + 1)Hₙ − 4n ≈ 2n ln n ≈ 1.39 n log₂ n

At n = 2²⁰ that is 26,088,935. The benchmark measured 25,440,847 on its single
random input, 2.5% fewer; the standard deviation of C(n) is about 0.65n
(Knuth, TAOCP vol. 3), so this is within one standard deviation.

The tuned `quick_sort` chooses the median of three elements as pivot, which
lowers the expected count to (12/7) n ln n ≈ 1.19 n log₂ n, but it also hands
ranges of up to 32 elements to insertion sort, which spends extra comparisons.
Measured: 1.24 n log₂ n, slightly _more_ comparisons than the textbook version,
and 11% _less_ time. Comparisons are not the cost that matters most here.

### Median of three is not enough

McIlroy's "A Killer Adversary for Quicksort" (1999) builds, for any
deterministic quicksort, an input that makes it quadratic. It runs the sort on
placeholder values and only decides what an element's value is when the
algorithm compares it, always so the pivot ends up near an extreme. The
`adversarial` benchmark input is that construction aimed at `aao::quick_sort`.
At n = 32,768 the tuned quicksort takes 1,647 ns per element instead of 2.4.

Introsort (Musser, 1997) caps the damage: it counts its recursion depth and
switches the current range to heap sort beyond 2 log₂ n levels. Heap sort is
Θ(n log n) in the worst case, so introsort is too: 45 ns per element on the
same input. This is the strategy behind `std::sort` in libstdc++, libc++ and
the MSVC standard library.

### Heap sort

Building the heap takes O(n); each of the n extractions sifts down through
about log₂ n levels with two comparisons per level, about 2n log₂ n in total.
Measured on random input: 1.85 n log₂ n.

### Summary of comparison counts at n = 2²⁰, random input

| Algorithm            | Comparisons | ÷ n log₂ n | ÷ log₂(n!) |
| -------------------- | ----------: | ---------: | ---------: |
| textbook::merge_sort |  19,645,004 |       0.94 |       1.01 |
| std::stable_sort     |  20,773,575 |       0.99 |       1.07 |
| merge_sort           |  24,741,008 |       1.18 |       1.27 |
| std::sort            |  25,273,223 |       1.21 |       1.30 |
| textbook::quick_sort |  25,440,847 |       1.21 |       1.31 |
| quick_sort           |  25,947,244 |       1.24 |       1.33 |
| heap_sort            |  38,705,995 |       1.85 |       1.99 |
| radix_sort           |           0 |          0 |          0 |

## Part 2: measuring time

If comparisons were the cost, the table above would be the ranking. It is not:

| Algorithm            | Comparisons (rank) |  Time at 2²⁰, random (rank) |
| -------------------- | -----------------: | --------------------------: |
| textbook::merge_sort |         fewest (1) | 107 ns/element (9, slowest) |
| quick_sort           |                  6 |           43 ns/element (2) |

The textbook merge sort makes the fewest comparisons of all and is the slowest
comparison sort measured. Three costs that comparison counts ignore explain the
difference.

**Allocation.** `textbook::merge_sort` creates two new vectors in every one of
its ~n merges. The tuned version allocates one buffer once. It makes 26% more
comparisons (insertion sort on the small runs) and takes half the time.

**Branch mispredictions.** A modern core guesses the outcome of each branch and
runs ahead; a wrong guess costs 15-20 cycles. On random data, "is a < b?" is a
coin flip, so a comparison sort pays roughly one misprediction per two
comparisons. Radix sort's inner loop has no data-dependent branches at all,
which is a large part of why it reaches 3.4 ns per element.

**Memory.** A comparison takes a fraction of a nanosecond when both values are
in the L1 cache, and around 80 ns when one has to come from main memory. That
ratio, not the comparison count, decides how fast a search is on a large array.

### Searching: same probes, different memory

Binary search makes ⌊log₂ n⌋ + 1 probes. Every variant in
`include/aao/search.hpp` makes about the same number. The time per lookup
(65,536 random lookups, `search.csv`):

| n   | Array size | std::lower_bound | branchless_lower_bound | eytzinger_index |
| --- | ---------: | ---------------: | ---------------------: | --------------: |
| 2¹⁰ |       4 KB |            36 ns |                   7 ns |            6 ns |
| 2²⁰ |       4 MB |           117 ns |                  49 ns |           20 ns |
| 2²⁶ |     256 MB |           397 ns |                 206 ns |           56 ns |

- **Branchless** turns the comparison into a conditional move. With no branch to
  predict, there is nothing to mispredict: 5x faster on a 4 KB array, 2.4x on
  4 MB. Beyond the cache, each probe still waits for memory.
- **Eytzinger** stores the same sorted values in breadth-first order (root,
  then its two children, then the four grandchildren...). The top four levels
  of the tree share one cache line and stay in cache across lookups, and
  because the 16 great-great-grandchildren of any node are adjacent, one
  prefetch fetches the line needed four steps ahead. At 256 MB, far larger than
  this machine's 96 MB L3 cache, that is 7x faster than `std::lower_bound` with
  the same number of comparisons.

The vertical lines on the [search chart](assets/search-scaling.svg) mark the L1,
L2 and L3 cache sizes. The curves bend where the array stops fitting.

### Interpolation search: O(log log n), until it is not

Instead of probing the middle, interpolation search estimates where the key
should be from the values at the ends of the range. On uniformly distributed
keys the expected number of probes is O(log log n) (Perl, Itai and Avni, 1978):
at 2²⁶ elements, about 9 ns per lookup, 45x faster than `std::lower_bound`.

On skewed keys (here 2³¹·u⁸ for uniform u, so most keys crowd near zero) the
estimate is wrong every time and the range shrinks by a few elements per probe:
O(n). The textbook version took 23 µs per lookup at only 2¹⁶ elements, 375x
slower than `std::lower_bound`. The guarded `interpolation_search` alternates
each interpolation probe with one bisection step, which bounds it at about
2 log₂ n probes on any input while keeping most of the uniform-case speed.

### A benchmarking lesson: the branch predictor learns your input

The first draft of this benchmark timed each sort on the same array over and over. On
modern processors that is a mistake. Sorting one identical 1,000-element array
thousands of times lets the branch predictor memorize its sequence of
comparison outcomes: `std::sort` measured 2.3 ns per element that way, against
22.7 ns on fresh arrays. The benchmark now cycles through 16 different arrays
for small sizes and draws 65,536 search keys per measurement, too many to
memorize.

### Another one: radix sort and cache sets

`radix_sort` takes 3.4 ns per element on 2²⁰ random keys but 14.9 ns on the
keys 0 … 2²⁰−1, already sorted. With those keys every one of the 256 buckets of
a pass holds exactly 4,096 elements, so the 256 write positions sit exactly
16 KB apart and advance in lockstep. Addresses 16 KB apart map to the same L1
cache sets, and 256 streams compete for 12 ways. Sorted keys with random gaps,
where bucket sizes vary slightly, run at full speed (3.1 ns), which isolates
the cause.

## What the analysis got right

- Every comparison count matched its formula to within a fraction of a percent,
  or exactly, for the worst cases.
- The asymptotic class predicted the shape of every time curve: flat lines in
  ns per element for O(n), slowly rising for O(n log n), steeply rising for O(n²).
- The worst-case analysis predicted precisely which inputs break which
  algorithm.

## What it missed

- The constants: on the same random input, three Θ(n log n) sorts differ by 2.5x in time.
- That the algorithm with the fewest comparisons can be the slowest.
- The memory hierarchy, which dominates search time on large arrays.

## References

- T. H. Cormen, C. E. Leiserson, R. L. Rivest, C. Stein. _Introduction to Algorithms_, 3rd ed. MIT Press, 2009.
- D. E. Knuth. _The Art of Computer Programming, Vol. 3: Sorting and Searching_, 2nd ed. Addison-Wesley, 1998.
- R. Sedgewick, K. Wayne. _Algorithms_, 4th ed. Addison-Wesley, 2011.
- M. D. McIlroy. "A Killer Adversary for Quicksort." _Software: Practice and Experience_ 29(4), 1999.
- D. R. Musser. "Introspective Sorting and Selection Algorithms." _Software: Practice and Experience_ 27(8), 1997.
- Y. Perl, A. Itai, H. Avni. "Interpolation Search: A Log Log N Search." _Communications of the ACM_ 21(7), 1978.
- P.-V. Khuong, P. Morin. "Array Layouts for Comparison-Based Searching." _ACM Journal of Experimental Algorithmics_ 22, 2017.
