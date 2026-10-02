// Searching a sorted array: where each algorithm looks, and which cache
// lines those looks land in. Ports of include/aao/search.hpp.

const N = 127; // a complete tree of 7 levels
const LINE = 16; // 4-byte keys per 64-byte cache line

const css = (name) =>
  getComputedStyle(document.documentElement).getPropertyValue(name).trim();

// Probe i of `count` sits a little lower than the one before it.
const probeY = (i, count) => 4 + (count > 1 ? (i / (count - 1)) * 16 : 0);

function makeData(kind) {
  const values = [];
  for (let i = 0; i < N; i++) {
    const u = (i + Math.random()) / N;
    values.push(
      kind === "skewed" ? Math.floor(10000 * u ** 8) : Math.floor(10000 * u),
    );
  }
  return values;
}

// Eytzinger (breadth-first) layout: e[1] is the root, children of k at 2k, 2k+1.
function eytzinger(sorted) {
  const e = new Array(sorted.length + 1);
  let next = 0;
  (function build(k) {
    if (k > sorted.length) return;
    build(2 * k);
    e[k] = sorted[next++];
    build(2 * k + 1);
  })(1);
  return e;
}

// Each returns the positions it probed, in order, in its own layout.
function binaryProbes(a, key) {
  const probes = [];
  let lo = 0;
  let hi = a.length;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    probes.push(mid);
    if (a[mid] < key) lo = mid + 1;
    else if (key < a[mid]) hi = mid;
    else break;
  }
  return probes;
}

function interpolate(lo, hi, loV, hiV, key) {
  const span = hi - 1 - lo;
  const den = hiV - loV;
  if (!(den > 0)) return lo + (span >> 1);
  return (
    lo + Math.min(span, Math.max(0, Math.floor(((key - loV) / den) * span)))
  );
}

function interpolationProbes(a, key, guarded) {
  const probes = [];
  let lo = 0;
  let hi = a.length;
  while (lo < hi) {
    if (!(a[lo] < key) || a[hi - 1] < key) break;
    const probe = interpolate(lo, hi, a[lo], a[hi - 1], key);
    probes.push(probe);
    if (a[probe] < key) lo = probe + 1;
    else hi = probe;
    if (!guarded || lo >= hi) continue;
    const mid = lo + ((hi - lo) >> 1);
    probes.push(mid);
    if (a[mid] < key) lo = mid + 1;
    else hi = mid;
  }
  return probes;
}

function eytzingerProbes(e, key) {
  const probes = [];
  let k = 1;
  while (k < e.length) {
    probes.push(k);
    k = 2 * k + (e[k] < key ? 1 : 0);
  }
  return probes;
}

// Where the answer (the first element not less than key) sits in each layout.
function lowerBound(a, key) {
  let lo = 0;
  let hi = a.length;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (a[mid] < key) lo = mid + 1;
    else hi = mid;
  }
  return lo;
}

function eytzingerAnswer(e, key) {
  let k = 1;
  while (k < e.length) k = 2 * k + (e[k] < key ? 1 : 0);
  while (k & 1) k >>= 1;
  return k >> 1;
}

const ROWS = [
  {
    id: "binary",
    name: "Binary search",
    layout: "sorted",
    probes: (s, key) => binaryProbes(s.sorted, key),
    caption: "Halves the range every step. Each probe is far from the last.",
  },
  {
    id: "interpolation-textbook",
    name: "Interpolation search, textbook",
    layout: "sorted",
    probes: (s, key) => interpolationProbes(s.sorted, key, false),
    caption: "Guesses the position from the values. Try the skewed keys.",
  },
  {
    id: "interpolation",
    name: "Interpolation search, guarded",
    layout: "sorted",
    probes: (s, key) => interpolationProbes(s.sorted, key, true),
    caption: "Alternates a guess with a halving step, so it can never crawl.",
  },
  {
    id: "eytzinger",
    name: "Eytzinger layout",
    layout: "eytzinger",
    probes: (s, key) => eytzingerProbes(s.eytz, key),
    caption:
      "Same values, stored breadth-first. Levels 1-4 share one cache line, and each later line is known four levels ahead, so it can be prefetched.",
  },
];

export function setupSearch(root) {
  const distButtons = [...root.querySelectorAll("[data-dist]")];
  const newTarget = root.querySelector("#search-target");
  const targetOut = root.querySelector("#search-target-out");
  const list = root.querySelector("#probe-rows");
  const template = root.querySelector("#probe-template");
  const calm = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  const state = {
    dist: "uniform",
    sorted: [],
    eytz: [],
    key: 0,
    timer: 0,
    shown: 0,
  };

  const rows = ROWS.map((row) => {
    const el = template.content.firstElementChild.cloneNode(true);
    el.querySelector(".probe-name").textContent = row.name;
    el.querySelector(".probe-caption").textContent = row.caption;
    list.append(el);
    const canvas = el.querySelector("canvas");
    const r = {
      ...row,
      el,
      canvas,
      ctx: canvas.getContext("2d"),
      stats: el.querySelector(".probe-stats"),
      path: [],
    };
    new ResizeObserver(() => draw(r)).observe(canvas);
    if (row.layout === "sorted") {
      canvas.addEventListener("click", (event) => {
        const box = canvas.getBoundingClientRect();
        const index = Math.min(
          N - 1,
          Math.floor(((event.clientX - box.left) / box.width) * N),
        );
        search(state.sorted[index]);
      });
      canvas.title = "Click a bar to search for its value";
    }
    return r;
  });

  function draw(r) {
    const { canvas, ctx } = r;
    const dpr = window.devicePixelRatio || 1;
    const { width, height } = canvas.getBoundingClientRect();
    if (!width) return;
    canvas.width = Math.round(width * dpr);
    canvas.height = Math.round(height * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, width, height);

    const eytz = r.layout === "eytzinger";
    const slots = eytz ? N + 1 : N; // the Eytzinger array keeps slot 0 empty
    const values = eytz ? state.eytz : state.sorted;
    const slot = width / slots;
    const top = 26;
    const plot = height - top;
    const max = Math.max(...state.sorted, 1);
    const shown = r.path.slice(0, state.shown);
    const touched = new Set(shown.map((p) => Math.floor(p / LINE)));

    // Cache lines this search touched, then the line boundaries.
    ctx.fillStyle = css("--range");
    for (const line of touched)
      ctx.fillRect(line * LINE * slot, top, LINE * slot, plot);
    ctx.fillStyle = css("--rule");
    for (let b = LINE; b < slots; b += LINE)
      ctx.fillRect(Math.round(b * slot), top, 1, plot);

    const done = shown.length === r.path.length;
    const answer = !done ? -1 : eytz ? eytzingerAnswer(state.eytz, state.key) : lowerBound(state.sorted, state.key);
    for (let k = 0; k < slots; k++) {
      if (values[k] === undefined) continue;
      const h = Math.max(1, (values[k] / max) * (plot - 2));
      const probed = shown.includes(k);
      ctx.fillStyle =
        k === answer ? css("--done") : probed ? css("--compare") : css("--bar");
      ctx.fillRect(
        k * slot + (slot > 3 ? 0.5 : 0),
        height - h,
        Math.max(1, slot - 1),
        h,
      );
    }

    // The order of the probes, as a path above the bars.
    if (shown.length) {
      ctx.strokeStyle = css("--compare");
      ctx.fillStyle = css("--compare");
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      shown.forEach((p, i) => {
        const x = (p + 0.5) * slot;
        const y = probeY(i, shown.length);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.stroke();
      shown.forEach((p, i) => {
        ctx.beginPath();
        ctx.arc((p + 0.5) * slot, probeY(i, shown.length), 2.5, 0, Math.PI * 2);
        ctx.fill();
      });
    }

    const lines = touched.size;
    r.stats.textContent = r.path.length
      ? `${r.path.length} ${r.path.length === 1 ? "probe" : "probes"}, ${lines} cache ${lines === 1 ? "line" : "lines"}`
      : "";
    r.stats.classList.toggle("bad", r.path.length > 16);
  }

  function search(key) {
    state.key = key;
    targetOut.textContent = String(key);
    for (const r of rows) r.path = r.probes(state, key);
    clearInterval(state.timer);
    const longest = Math.max(...rows.map((r) => r.path.length));
    state.shown = calm ? longest : 0;
    rows.forEach(draw);
    if (calm) return;
    state.timer = setInterval(() => {
      state.shown++;
      rows.forEach(draw);
      if (state.shown >= longest) clearInterval(state.timer);
    }, 160);
  }

  function randomTarget() {
    search(state.sorted[Math.floor(Math.random() * N)]);
  }

  function load(dist) {
    state.dist = dist;
    for (const b of distButtons)
      b.setAttribute("aria-pressed", String(b.dataset.dist === dist));
    state.sorted = makeData(dist);
    state.eytz = eytzinger(state.sorted);
    randomTarget();
  }

  for (const b of distButtons)
    b.addEventListener("click", () => load(b.dataset.dist));
  newTarget.addEventListener("click", randomTarget);
  load("uniform");
}
