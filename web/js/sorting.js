// Instrumented JavaScript ports of include/aao/sort.hpp.
//
// Each algorithm runs to completion against a Recorder, which logs every
// comparison and every element move. The page then replays the log at any
// speed, forwards or backwards. One log entry is one step of work.

export const CMP = 0; // compared two elements
export const MOVE = 1; // wrote one element
export const SWAP = 2; // exchanged two elements
export const READ = 3; // read or copied an element out (radix histogram, merge buffers)

// Same cutoff as aao::detail::kSmallSort.
export const SMALL_SORT = 32;

export class Recorder {
  constructor(values) {
    this.a = values.slice();
    this.log = [];
    this.line = -1;
    this.lo = 0;
    this.hi = values.length;
  }

  // Ordering used by every comparison; the adversary overrides it.
  lt(x, y) {
    return x < y;
  }

  push(op, i, j, from, to) {
    this.log.push({
      op,
      i,
      j,
      from,
      to,
      line: this.line,
      lo: this.lo,
      hi: this.hi,
    });
  }

  focus(lo, hi) {
    this.lo = lo;
    this.hi = hi;
  }

  // a[i] < a[j]
  less(i, j) {
    this.push(CMP, i, j);
    return this.lt(this.a[i], this.a[j]);
  }

  // held value v < a[j]. For comparisons against values held outside the
  // array, `from`/`to` record what was compared, for the narration.
  heldLess(v, j) {
    this.push(CMP, j, -1, v, this.a[j]);
    return this.lt(v, this.a[j]);
  }

  // a[i] < held value v
  lessHeld(i, v) {
    this.push(CMP, i, -1, this.a[i], v);
    return this.lt(this.a[i], v);
  }

  // x < y for two values held outside the array, deciding position k.
  heldPair(k, x, y) {
    this.push(CMP, k, -1, x, y);
    return this.lt(x, y);
  }

  set(i, v) {
    this.push(MOVE, i, -1, this.a[i], v);
    this.a[i] = v;
  }

  swap(i, j) {
    this.push(SWAP, i, j);
    const t = this.a[i];
    this.a[i] = this.a[j];
    this.a[j] = t;
  }

  read(i) {
    this.push(READ, i, -1);
    return this.a[i];
  }
}

// ---------------------------------------------------------------------------

function insertion(r, lo, hi, line) {
  for (let i = lo + 1; i < hi; i++) {
    r.line = line.key;
    const key = r.a[i];
    let j = i;
    r.line = line.compare;
    while (j > lo && r.heldLess(key, j - 1)) {
      r.line = line.shift;
      r.set(j, r.a[j - 1]);
      j--;
      r.line = line.compare;
    }
    r.line = line.place;
    r.set(j, key);
  }
}

// Heap positions are relative to `base`, so introsort can heap-sort a subrange.
function siftDown(r, base, root, n, line) {
  r.line = line.take;
  const value = r.a[base + root];
  for (;;) {
    let child = 2 * root + 1;
    if (child >= n) break;
    r.line = line.pick;
    if (child + 1 < n && r.less(base + child, base + child + 1)) child++;
    r.line = line.stop;
    if (!r.heldLess(value, base + child)) break;
    r.line = line.lift;
    r.set(base + root, r.a[base + child]);
    root = child;
  }
  r.line = line.place;
  r.set(base + root, value);
}

function heap(r, lo, hi, line) {
  const n = hi - lo;
  r.focus(lo, hi);
  r.line = line.build;
  for (let i = (n >> 1) - 1; i >= 0; i--) siftDown(r, lo, i, n, line);
  for (let end = n - 1; end > 0; end--) {
    r.line = line.swap;
    r.swap(lo, lo + end);
    r.focus(lo, lo + end);
    siftDown(r, lo, 0, end, line);
  }
}

function medianOfThreePartition(r, lo, hi, line) {
  const mid = lo + ((hi - lo) >> 1);
  r.line = line.median;
  if (r.less(mid, lo)) r.swap(lo, mid);
  if (r.less(hi - 1, mid)) {
    r.swap(mid, hi - 1);
    if (r.less(mid, lo)) r.swap(lo, mid);
  }
  const pivot = r.a[mid];
  let i = lo;
  let j = hi - 1;
  for (;;) {
    r.line = line.scanUp;
    do i++;
    while (r.lessHeld(i, pivot));
    r.line = line.scanDown;
    do j--;
    while (r.heldLess(pivot, j));
    if (i >= j) return i;
    r.line = line.swap;
    r.swap(i, j);
  }
}

function quickLoop(r, lo, hi, line, depth) {
  while (hi - lo > SMALL_SORT) {
    r.focus(lo, hi);
    if (depth === 0) {
      r.line = line.fallback;
      heap(r, lo, hi, HEAP_IN_INTRO);
      return;
    }
    if (depth > 0) depth--;
    const cut = medianOfThreePartition(r, lo, hi, line);
    r.line = line.recurse;
    if (cut - lo < hi - cut) {
      quickLoop(r, lo, cut, line, depth);
      lo = cut;
    } else {
      quickLoop(r, cut, hi, line, depth);
      hi = cut;
    }
  }
  r.focus(lo, hi);
  insertion(r, lo, hi, line.insertion);
}

// ---------------------------------------------------------------------------
// Algorithms, with the C++ they port (simplified from include/aao/sort.hpp).

const INSERTION_CODE = [
  "void insertion_sort(It first, It last) {",
  "    for (It i = first + 1; i != last; ++i) {",
  "        auto key = std::move(*i);",
  "        It j = i;",
  "        for (; j != first && key < *(j - 1); --j)",
  "            *j = std::move(*(j - 1));",
  "        *j = std::move(key);",
  "    }",
  "}",
];

const HEAP_CODE = [
  "void heap_sort(It first, It last) {",
  "    for (auto i = n / 2 - 1; i >= 0; --i) sift_down(first, i, n);",
  "    for (auto end = n - 1; end > 0; --end) {",
  "        std::iter_swap(first, first + end);",
  "        sift_down(first, 0, end);",
  "    }",
  "}",
  "",
  "void sift_down(It first, diff_t root, diff_t n) {",
  "    auto value = std::move(first[root]);",
  "    for (;;) {",
  "        diff_t child = 2 * root + 1;",
  "        if (child >= n) break;",
  "        if (child + 1 < n && first[child] < first[child + 1]) ++child;",
  "        if (!(value < first[child])) break;",
  "        first[root] = std::move(first[child]);",
  "        root = child;",
  "    }",
  "    first[root] = std::move(value);",
  "}",
];
const HEAP_LINES = {
  build: 1,
  swap: 3,
  take: 9,
  pick: 13,
  stop: 14,
  lift: 15,
  place: 18,
};
// When introsort falls back to heap sort, keep pointing at its own listing.
const HEAP_IN_INTRO = {
  build: 2,
  swap: 2,
  take: 2,
  pick: 2,
  stop: 2,
  lift: 2,
  place: 2,
};

const MERGE_TEXTBOOK_CODE = [
  "void merge_sort(It first, It last) {",
  "    if (last - first < 2) return;",
  "    It mid = first + (last - first) / 2;",
  "    merge_sort(first, mid);",
  "    merge_sort(mid, last);",
  "    std::vector left(first, mid), right(mid, last);  // 2 allocations",
  "    auto l = left.begin(), r = right.begin();",
  "    It out = first;",
  "    while (l != left.end() && r != right.end())",
  "        *out++ = *r < *l ? *r++ : *l++;",
  "    out = std::copy(l, left.end(), out);",
  "    std::copy(r, right.end(), out);",
  "}",
];

const MERGE_CODE = [
  "void merge_sort(It first, It last) {",
  "    std::vector<T> buf(n / 2);         // one buffer, allocated once",
  "    impl(first, last, buf.data());",
  "}",
  "",
  "void impl(It first, It last, T* buf) {",
  "    if (last - first <= 32) return insertion_sort(first, last);",
  "    It mid = first + (last - first) / 2;",
  "    impl(first, mid, buf);",
  "    impl(mid, last, buf);",
  "    if (!(*mid < *(mid - 1))) return;  // runs already in order",
  "    T* end = std::move(first, mid, buf);  // only the left run moves out",
  "    T* l = buf; It r = mid, out = first;",
  "    while (l != end && r != last)",
  "        *out++ = *r < *l ? std::move(*r++) : std::move(*l++);",
  "    std::move(l, end, out);",
  "}",
];

const QUICK_TEXTBOOK_CODE = [
  "void quick_sort(It first, It last) {",
  "    if (last - first < 2) return;",
  "    It pivot = last - 1;                 // last element as pivot",
  "    It store = first;",
  "    for (It j = first; j != pivot; ++j)",
  "        if (*j < *pivot) std::iter_swap(store++, j);",
  "    std::iter_swap(store, pivot);",
  "    quick_sort(first, store);",
  "    quick_sort(store + 1, last);",
  "}",
];

const QUICK_CODE = [
  "void quick_sort(It first, It last) {",
  "    while (last - first > 32) {",
  "        It mid = first + (last - first) / 2;",
  "        sort3(first, mid, last - 1);     // median of three",
  "        auto pivot = *mid;",
  "        It i = first, j = last - 1;",
  "        for (;;) {                       // Hoare partition",
  "            do ++i; while (*i < pivot);",
  "            do --j; while (pivot < *j);",
  "            if (i >= j) break;",
  "            std::iter_swap(i, j);",
  "        }",
  "        // recurse into the smaller side, loop on the larger",
  "        if (i - first < last - i) quick_sort(first, i), first = i;",
  "        else                      quick_sort(i, last), last = i;",
  "    }",
  "    insertion_sort(first, last);",
  "}",
];
const QUICK_LINES = {
  median: 3,
  scanUp: 7,
  scanDown: 8,
  swap: 10,
  recurse: 13,
  fallback: -1,
  insertion: { key: 16, compare: 16, shift: 16, place: 16 },
};

const INTRO_CODE = [
  "void intro_sort(It first, It last, int depth = 2 * log2(n)) {",
  "    while (last - first > 32) {",
  "        if (depth-- == 0) return heap_sort(first, last);",
  "        It cut = partition(first, last);   // median of 3 + Hoare",
  "        if (cut - first < last - cut) intro_sort(first, cut, depth), first = cut;",
  "        else                          intro_sort(cut, last, depth), last = cut;",
  "    }",
  "    insertion_sort(first, last);",
  "}",
];
const INTRO_LINES = {
  median: 3,
  scanUp: 3,
  scanDown: 3,
  swap: 3,
  recurse: 4,
  fallback: 2,
  insertion: { key: 7, compare: 7, shift: 7, place: 7 },
};

const RADIX_CODE = [
  "void radix_sort(It first, It last) {  // LSD; 4-bit digits here, bytes in the library",
  "    for (int shift = 0; shift < bits; shift += 4) {",
  "        size_t count[16] = {};",
  "        for (auto x : a) ++count[(x >> shift) & 15];      // histogram",
  "        exclusive_prefix_sum(count);                      // bucket starts",
  "        for (auto x : a) out[count[(x >> shift) & 15]++] = x;  // scatter",
  "        std::swap(a, out);",
  "    }",
  "}",
];

function mergeTextbook(r, lo, hi) {
  if (hi - lo < 2) return;
  const mid = lo + ((hi - lo) >> 1);
  r.line = 3;
  mergeTextbook(r, lo, mid);
  r.line = 4;
  mergeTextbook(r, mid, hi);
  r.focus(lo, hi);
  r.line = 5;
  const left = [];
  const right = [];
  for (let i = lo; i < mid; i++) left.push(r.read(i));
  for (let i = mid; i < hi; i++) right.push(r.read(i));
  let l = 0;
  let k = 0;
  let out = lo;
  while (l < left.length && k < right.length) {
    r.line = 9;
    if (r.heldPair(out, right[k], left[l])) r.set(out++, right[k++]);
    else r.set(out++, left[l++]);
  }
  r.line = 10;
  while (l < left.length) r.set(out++, left[l++]);
  r.line = 11;
  while (k < right.length) r.set(out++, right[k++]);
}

const MERGE_INSERTION = { key: 6, compare: 6, shift: 6, place: 6 };

function mergeTuned(r, lo, hi) {
  r.focus(lo, hi);
  if (hi - lo <= SMALL_SORT) {
    insertion(r, lo, hi, MERGE_INSERTION);
    return;
  }
  const mid = lo + ((hi - lo) >> 1);
  r.line = 8;
  mergeTuned(r, lo, mid);
  r.line = 9;
  mergeTuned(r, mid, hi);
  r.focus(lo, hi);
  r.line = 10;
  if (!r.less(mid, mid - 1)) return;
  r.line = 11;
  const buf = [];
  for (let i = lo; i < mid; i++) buf.push(r.read(i));
  let l = 0;
  let k = mid;
  let out = lo;
  while (l < buf.length && k < hi) {
    r.line = 14;
    if (r.lessHeld(k, buf[l])) r.set(out++, r.a[k++]);
    else r.set(out++, buf[l++]);
  }
  r.line = 15;
  while (l < buf.length) r.set(out++, buf[l++]);
}

function quickTextbook(r, lo, hi) {
  if (hi - lo < 2) return;
  r.focus(lo, hi);
  const pivot = hi - 1;
  let store = lo;
  for (let j = lo; j < pivot; j++) {
    r.line = 5;
    if (r.less(j, pivot)) r.swap(store++, j);
  }
  r.line = 6;
  r.swap(store, pivot);
  r.line = 7;
  quickTextbook(r, lo, store);
  r.line = 8;
  quickTextbook(r, store + 1, hi);
}

function radix(r) {
  const n = r.a.length;
  let max = 0;
  for (const x of r.a) max = Math.max(max, x);
  for (let shift = 0; max >> shift > 0; shift += 4) {
    const count = new Array(16).fill(0);
    r.line = 3;
    for (let i = 0; i < n; i++) count[(r.read(i) >> shift) & 15]++;
    if (count.includes(n)) continue; // every key has the same digit: skip the pass
    r.line = 4;
    let offset = 0;
    for (let d = 0; d < 16; d++) {
      const c = count[d];
      count[d] = offset;
      offset += c;
    }
    r.line = 5;
    const src = r.a.slice();
    for (const x of src) r.set(count[(x >> shift) & 15]++, x);
  }
}

export const ALGORITHMS = [
  {
    id: "insertion",
    name: "Insertion sort",
    code: INSERTION_CODE,
    note: "Quadratic, but the fastest choice for tiny ranges.",
    run: (r) =>
      insertion(r, 0, r.a.length, { key: 2, compare: 4, shift: 5, place: 6 }),
  },
  {
    id: "merge-textbook",
    name: "Merge sort, textbook",
    code: MERGE_TEXTBOOK_CODE,
    note: "Copies both halves out on every merge.",
    run: (r) => mergeTextbook(r, 0, r.a.length),
  },
  {
    id: "merge",
    name: "Merge sort, tuned",
    code: MERGE_CODE,
    note: "Insertion sort for small runs; skips merges that are already in order.",
    run: (r) => mergeTuned(r, 0, r.a.length),
  },
  {
    id: "quick-textbook",
    name: "Quicksort, textbook",
    code: QUICK_TEXTBOOK_CODE,
    note: "Last element as pivot. Quadratic on sorted input.",
    run: (r) => quickTextbook(r, 0, r.a.length),
  },
  {
    id: "quick",
    name: "Quicksort, tuned",
    code: QUICK_CODE,
    note: "Median-of-three pivot and Hoare partition.",
    run: (r) => quickLoop(r, 0, r.a.length, QUICK_LINES, -1),
  },
  {
    id: "intro",
    name: "Introsort",
    code: INTRO_CODE,
    note: "Quicksort that switches to heap sort when it recurses too deep.",
    run: (r) =>
      quickLoop(
        r,
        0,
        r.a.length,
        INTRO_LINES,
        2 * Math.floor(Math.log2(Math.max(2, r.a.length))),
      ),
  },
  {
    id: "heap",
    name: "Heap sort",
    code: HEAP_CODE,
    note: "Guaranteed n log n, but it jumps around memory.",
    run: (r) => heap(r, 0, r.a.length, HEAP_LINES),
  },
  {
    id: "radix",
    name: "Radix sort",
    code: RADIX_CODE,
    note: "Never compares two elements. One pass per digit.",
    run: (r) => radix(r),
  },
];

export function record(algorithm, values) {
  const r = new Recorder(values);
  algorithm.run(r);
  return r.log;
}

// ---------------------------------------------------------------------------
// Inputs

export const INPUTS = [
  { id: "random", name: "Random" },
  { id: "sorted", name: "Sorted" },
  { id: "reversed", name: "Reversed" },
  { id: "nearly", name: "Nearly sorted" },
  { id: "few", name: "Few unique" },
  { id: "adversarial", name: "Quicksort killer" },
];

function shuffle(a) {
  for (let i = a.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
}

// McIlroy's adversary (1999), aimed at the tuned quicksort above: values are
// decided lazily during the sort so that every pivot lands near an extreme.
// See bench/inputs.hpp for the C++ version used by the benchmark.
function quicksortKiller(n) {
  const gas = n - 1;
  const val = new Array(n).fill(gas);
  let solid = 0;
  let candidate = 0;
  const r = new Recorder(Array.from({ length: n }, (_, i) => i));
  r.lt = (x, y) => {
    if (val[x] === gas && val[y] === gas)
      val[x === candidate ? x : y] = solid++;
    if (val[x] === gas) candidate = x;
    else if (val[y] === gas) candidate = y;
    return val[x] < val[y];
  };
  quickLoop(r, 0, n, QUICK_LINES, -1);
  return val.map((v) => v + 1);
}

export function makeInput(kind, n) {
  const ascending = Array.from({ length: n }, (_, i) => i + 1);
  switch (kind) {
    case "sorted":
      return ascending;
    case "reversed":
      return ascending.reverse();
    case "nearly": {
      for (let s = 0; s < Math.max(1, Math.floor(n / 20)); s++) {
        const i = Math.floor(Math.random() * n);
        const j = Math.floor(Math.random() * n);
        [ascending[i], ascending[j]] = [ascending[j], ascending[i]];
      }
      return ascending;
    }
    case "few":
      return Array.from({ length: n }, () =>
        Math.ceil(((Math.floor(Math.random() * 6) + 1) * n) / 6),
      );
    case "adversarial":
      return quicksortKiller(n);
    default:
      return shuffle(ascending);
  }
}

// ---------------------------------------------------------------------------
// Replay

export class Player {
  constructor(values, log) {
    this.initial = values.slice();
    this.a = values.slice();
    this.log = log;
    this.pos = 0;
    this.comparisons = 0;
    this.moves = 0;
  }

  get done() {
    return this.pos >= this.log.length;
  }

  get current() {
    return this.pos > 0 ? this.log[this.pos - 1] : null;
  }

  forward() {
    if (this.done) return null;
    const e = this.log[this.pos++];
    if (e.op === MOVE) this.a[e.i] = e.to;
    else if (e.op === SWAP) this.swap(e.i, e.j);
    this.count(e, 1);
    return e;
  }

  backward() {
    if (this.pos === 0) return null;
    const e = this.log[--this.pos];
    if (e.op === MOVE) this.a[e.i] = e.from;
    else if (e.op === SWAP) this.swap(e.i, e.j);
    this.count(e, -1);
    return e;
  }

  seek(target) {
    const t = Math.max(0, Math.min(this.log.length, target));
    while (this.pos < t) this.forward();
    while (this.pos > t) this.backward();
  }

  advance(steps) {
    for (let s = 0; s < steps && !this.done; s++) this.forward();
  }

  swap(i, j) {
    const t = this.a[i];
    this.a[i] = this.a[j];
    this.a[j] = t;
  }

  count(e, sign) {
    if (e.op === CMP) this.comparisons += sign;
    else if (e.op === MOVE) this.moves += sign;
    else if (e.op === SWAP) this.moves += 2 * sign;
  }
}
