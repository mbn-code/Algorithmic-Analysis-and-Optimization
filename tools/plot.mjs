#!/usr/bin/env node
// Renders the README charts from results/*.csv into docs/assets/*.svg.
//
//   node tools/plot.mjs [results-dir]
//
// No dependencies. Series styles are shared with the web page, so a line has
// the same color and dash pattern everywhere.

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { SERIES } from "../web/js/charts.js";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const resultsDir = process.argv[2] ?? join(root, "results");
const outDir = join(root, "docs", "assets");

// Same values as the custom properties in web/style.css.
const COLOR = {
  "--paper": "#e9f1f8",
  "--c1": "#d95926",
  "--c2": "#199e70",
  "--c3": "#c98500",
  "--c4": "#d55181",
  "--c5": "#9085e9",
};
const BG = "#0b2340";
const INK = "#e9f1f8";
const MUTED = "#a9bfd4";
const RULE = "rgba(233,241,248,0.16)";
const FONT = "Archivo, 'Segoe UI', -apple-system, Helvetica, Arial, sans-serif";
const MONO =
  "'JetBrains Mono', ui-monospace, 'SFMono-Regular', Menlo, Consolas, monospace";

function readCsv(file) {
  const [head, ...rows] = readFileSync(join(resultsDir, file), "utf8")
    .trim()
    .split(/\r?\n/);
  const keys = head.split(",");
  return rows.map((row) => {
    const cells = row.split(",");
    return Object.fromEntries(
      keys.map((k, i) => [
        k,
        cells[i] !== "" && !Number.isNaN(Number(cells[i]))
          ? Number(cells[i])
          : cells[i],
      ]),
    );
  });
}

const meta = JSON.parse(readFileSync(join(resultsDir, "meta.json"), "utf8"));
const sort = readCsv("sort.csv");
const search = readCsv("search.csv");

const esc = (s) =>
  String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
const fmtN = (n) =>
  n >= 1 << 20 ? `${n / (1 << 20)}M` : n >= 1024 ? `${n / 1024}K` : String(n);
const fmtV = (v) =>
  v >= 100 ? v.toFixed(0) : v >= 10 ? v.toFixed(1) : v.toFixed(2);
const fmtBytes = (b) =>
  b >= 1 << 20 ? `${b / (1 << 20)} MB` : `${b / 1024} KB`;
const color = (name) => COLOR[(SERIES[name] ?? { color: "--paper" }).color];
const dash = (name) => (SERIES[name] ?? { dash: "" }).dash;
const machine = `${meta.cpu}, ${meta.compiler.replace(/^gcc/, "GCC")}, ${meta.date}`;

function frame(width, height, title, subtitle, body, label) {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" role="img" aria-label="${esc(label)}">
<title>${esc(label)}</title>
<rect width="${width}" height="${height}" rx="6" fill="${BG}"/>
<text x="28" y="40" fill="${INK}" font-family="${FONT}" font-size="19" font-weight="700">${esc(title)}</text>
<text x="28" y="62" fill="${MUTED}" font-family="${FONT}" font-size="13">${esc(subtitle)}</text>
${body}
</svg>
`;
}

// Keeps right-hand labels at least `gap` pixels apart, preserving their order.
function spread(labels, gap, lo, hi) {
  const sorted = [...labels].sort((a, b) => a.y - b.y);
  for (let i = 1; i < sorted.length; i++)
    sorted[i].y = Math.max(sorted[i].y, sorted[i - 1].y + gap);
  const overflow = sorted.length ? sorted.at(-1).y - hi : 0;
  if (overflow > 0) for (const l of sorted) l.y -= overflow;
  for (let i = sorted.length - 2; i >= 0; i--)
    sorted[i].y = Math.min(sorted[i].y, sorted[i + 1].y - gap);
  for (const l of sorted) l.y = Math.max(l.y, lo);
  return sorted;
}

// A log-log line chart with direct labels at the line ends.
function linePanel({
  x0,
  y0,
  w,
  h,
  rows,
  names,
  metric,
  nMin,
  nMax,
  caches = [],
  unitLabel,
  title,
}) {
  const values = rows
    .filter((r) => names.includes(r.algorithm))
    .map((r) => r[metric]);
  const lo = Math.floor(Math.log10(Math.min(...values) * 0.8));
  const hi = Math.log10(Math.max(...values) * 1.3);
  const x = (n) =>
    x0 +
    ((Math.log2(n) - Math.log2(nMin)) / (Math.log2(nMax) - Math.log2(nMin))) *
      w;
  const y = (v) => y0 + (1 - (Math.log10(v) - lo) / (hi - lo)) * h;
  let out = "";
  if (title)
    out += `<text x="${x0}" y="${y0 - 14}" fill="${INK}" font-family="${FONT}" font-size="14" font-weight="700">${esc(title)}</text>`;
  for (let e = lo; e <= Math.ceil(hi); e++) {
    for (const k of [1, 2, 5]) {
      const v = k * 10 ** e;
      if (Math.log10(v) < lo || Math.log10(v) > hi) continue;
      out += `<line x1="${x0}" x2="${x0 + w}" y1="${y(v).toFixed(1)}" y2="${y(v).toFixed(1)}" stroke="${RULE}"/>`;
      out += `<text x="${x0 - 8}" y="${(y(v) + 4).toFixed(1)}" text-anchor="end" fill="${MUTED}" font-family="${FONT}" font-size="11.5">${v >= 1000 ? `${v / 1000}k` : v}</text>`;
    }
  }
  for (let e = Math.log2(nMin); e <= Math.log2(nMax); e += 2) {
    const n = 2 ** e;
    out += `<text x="${x(n).toFixed(1)}" y="${y0 + h + 20}" text-anchor="middle" fill="${MUTED}" font-family="${FONT}" font-size="11.5">${fmtN(n)}</text>`;
  }
  out += `<line x1="${x0}" x2="${x0 + w}" y1="${y0 + h}" y2="${y0 + h}" stroke="${MUTED}"/>`;
  out += `<text x="${x0 + w}" y="${y0 + h + 40}" text-anchor="end" fill="${MUTED}" font-family="${FONT}" font-size="12">elements (n)</text>`;
  out += `<text x="${x0}" y="${y0 + h + 40}" fill="${MUTED}" font-family="${FONT}" font-size="12">${esc(unitLabel)}</text>`;
  for (const c of caches) {
    const n = c.bytes / 4;
    if (n <= nMin || n >= nMax) continue;
    out += `<line x1="${x(n).toFixed(1)}" x2="${x(n).toFixed(1)}" y1="${y0}" y2="${y0 + h}" stroke="${INK}" stroke-opacity="0.3"/>`;
    out += `<text x="${(x(n) + 5).toFixed(1)}" y="${y0 + 12}" fill="${INK}" fill-opacity="0.8" font-family="${FONT}" font-size="11.5">${c.level} ${fmtBytes(c.bytes)}</text>`;
  }
  const labels = [];
  for (const name of names) {
    const pts = rows
      .filter((r) => r.algorithm === name && r.n >= nMin && r.n <= nMax)
      .sort((a, b) => a.n - b.n);
    if (!pts.length) continue;
    const d = pts
      .map(
        (r, i) =>
          `${i ? "L" : "M"}${x(r.n).toFixed(1)},${y(r[metric]).toFixed(1)}`,
      )
      .join("");
    out += `<path d="${d}" fill="none" stroke="${color(name)}" stroke-width="2" stroke-dasharray="${dash(name)}" stroke-linejoin="round"/>`;
    const end = pts.at(-1);
    labels.push({ name, x: x(end.n), y: y(end[metric]), v: end[metric] });
  }
  for (const l of spread(labels, 15, y0 + 4, y0 + h)) {
    out += `<line x1="${l.x + 4}" x2="${l.x + 14}" y1="${l.y}" y2="${l.y}" stroke="${color(l.name)}" stroke-width="2" stroke-dasharray="${dash(l.name) ? "3 2" : ""}"/>`;
    out += `<text x="${l.x + 18}" y="${l.y + 4}" fill="${INK}" font-family="${MONO}" font-size="11.5">${esc(l.name)} <tspan fill="${MUTED}">${fmtV(l.v)}</tspan></text>`;
  }
  return out;
}

function hatch(id, stroke) {
  return `<pattern id="${id}" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="6" height="6" fill="${stroke}" fill-opacity="0.25"/><line x1="0" y1="0" x2="0" y2="6" stroke="${stroke}" stroke-width="3"/></pattern>`;
}

// ---------------------------------------------------------------------------
// 1. Sorting 1M random integers: one bar per algorithm.

function sortBars() {
  const n = 1 << 20;
  const rows = sort
    .filter((r) => r.distribution === "random" && r.n === n)
    .sort((a, b) => a.ns_per_element - b.ns_per_element);
  const width = 880;
  const top = 92;
  const rowH = 32;
  const labelW = 210;
  const barMax = width - labelW - 150;
  const max = Math.max(...rows.map((r) => r.ns_per_element));
  const reference = rows.find(
    (r) => r.algorithm === "std::sort",
  ).ns_per_element;
  let defs = "";
  let body = "";
  rows.forEach((r, i) => {
    const y = top + i * rowH;
    const textbook = r.algorithm.startsWith("textbook::");
    const fill = textbook ? `url(#h${i})` : color(r.algorithm);
    if (textbook) defs += hatch(`h${i}`, color(r.algorithm));
    const w = Math.max(3, (r.ns_per_element / max) * barMax);
    const ratio = r.ns_per_element / reference;
    const note =
      r.algorithm === "std::sort"
        ? "baseline"
        : Math.abs(ratio - 1) < 0.05
          ? "about the same"
          : ratio < 1
            ? `${(1 / ratio).toFixed(1)}x faster`
            : `${ratio.toFixed(1)}x slower`;
    body += `<text x="${labelW - 12}" y="${y + 19}" text-anchor="end" fill="${INK}" font-family="${MONO}" font-size="13">${esc(r.algorithm)}</text>`;
    body += `<rect x="${labelW}" y="${y + 5}" width="${w.toFixed(1)}" height="20" rx="2" fill="${fill}"/>`;
    body += `<text x="${(labelW + w + 10).toFixed(1)}" y="${y + 19}" fill="${INK}" font-family="${FONT}" font-size="13" font-weight="600">${fmtV(r.ns_per_element)} ns <tspan fill="${MUTED}" font-weight="400">${note}</tspan></text>`;
  });
  const height = top + rows.length * rowH + 40;
  body += `<text x="28" y="${height - 16}" fill="${MUTED}" font-family="${FONT}" font-size="12">Hatched: textbook versions. insertion_sort is not run at this size; it is quadratic. ${esc(machine)}.</text>`;
  return frame(
    width,
    height,
    "Sorting 1M random 32-bit integers",
    "Median nanoseconds per element, lower is better. Compared with std::sort.",
    `<defs>${defs}</defs>${body}`,
    "Bar chart: time per element to sort one million random integers, per algorithm",
  );
}

// ---------------------------------------------------------------------------
// 2. Searching: time per lookup across sizes, with the cache boundaries.

function searchLines() {
  const width = 880;
  const height = 470;
  const rows = search.filter((r) => r.distribution === "uniform");
  const names = [
    "std::lower_bound",
    "textbook::binary_search",
    "branchless_lower_bound",
    "eytzinger_index",
    "interpolation_search",
    "textbook::interpolation_search",
  ];
  const sizes = rows.map((r) => r.n);
  const body = linePanel({
    x0: 64,
    y0: 96,
    w: width - 64 - 290,
    h: height - 96 - 64,
    rows,
    names,
    metric: "median_ns_per_query",
    nMin: Math.min(...sizes),
    nMax: Math.max(...sizes),
    caches: meta.caches ?? [],
    unitLabel: "ns per lookup (log scale)",
  });
  return frame(
    width,
    height,
    "Looking up random keys in a sorted array",
    `Median nanoseconds per lookup over 65,536 random uniform keys. Vertical lines: cache sizes. ${meta.cpu}.`,
    body,
    "Line chart: nanoseconds per lookup against array size for six search algorithms",
  );
}

// ---------------------------------------------------------------------------
// 3. Quadratic traps: two inputs that turn a quicksort quadratic.

function traps() {
  const width = 960;
  const height = 400;
  const panelW = 240;
  const sorted = sort.filter((r) => r.distribution === "sorted");
  const killer = sort.filter((r) => r.distribution === "adversarial");
  const left = linePanel({
    x0: 60,
    y0: 112,
    w: panelW,
    h: 220,
    rows: sorted,
    names: ["textbook::quick_sort", "std::sort", "quick_sort"],
    metric: "ns_per_element",
    nMin: 1024,
    nMax: 32768,
    unitLabel: "ns per element",
    title: "Already sorted input",
  });
  const right = linePanel({
    x0: 60 + panelW + 240,
    y0: 112,
    w: panelW,
    h: 220,
    rows: killer,
    names: ["quick_sort", "intro_sort", "std::sort", "merge_sort"],
    metric: "ns_per_element",
    nMin: 1024,
    nMax: 32768,
    unitLabel: "ns per element",
    title: "McIlroy's quicksort killer",
  });
  return frame(
    width,
    height,
    "Two inputs that make quicksort quadratic",
    "Last-element pivots fail on sorted input; median-of-three fails on an adversary. Introsort caps the damage.",
    left + right,
    "Two line charts: quicksort variants slowing down quadratically on sorted and adversarial input",
  );
}

mkdirSync(outDir, { recursive: true });
const outputs = {
  "sort-1m.svg": sortBars(),
  "search-scaling.svg": searchLines(),
  "quadratic-traps.svg": traps(),
};
for (const [name, svg] of Object.entries(outputs)) {
  writeFileSync(join(outDir, name), svg);
  console.log(`wrote docs/assets/${name}`);
}
