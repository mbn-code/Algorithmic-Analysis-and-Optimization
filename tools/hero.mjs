#!/usr/bin/env node
// Renders docs/assets/race.svg: six sorting algorithms racing on the same
// array, as a looping animated SVG (SMIL, so it plays inside a README image).
//
//   node tools/hero.mjs
//
// Uses the same instrumented algorithms as the web page (web/js/sorting.js):
// one tick per comparison or element move.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ALGORITHMS, record, MOVE, SWAP } from "../web/js/sorting.js";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const LANES = [
  "insertion",
  "merge-textbook",
  "quick-textbook",
  "heap",
  "intro",
  "radix",
];
const N = 64;
const RUN = 10; // seconds for the slowest algorithm
const HOLD = 3; // seconds to show the finished state before looping

const BG = "#0b2340";
const PANEL = "#0f2c4c";
const INK = "#e9f1f8";
const MUTED = "#a9bfd4";
const BAR = "#a9c0d7";
const DONE = "#7fdccf";
const CURSOR = "#ff8a7a";
const RULE = "rgba(233,241,248,0.16)";
const FONT = "Archivo, 'Segoe UI', -apple-system, Helvetica, Arial, sans-serif";

// A fixed shuffle, so the image is reproducible.
function shuffled(n) {
  const a = Array.from({ length: n }, (_, i) => i + 1);
  let seed = 20241219;
  for (let i = n - 1; i > 0; i--) {
    seed = (seed * 1103515245 + 12345) % 2147483648;
    const j = seed % (i + 1);
    [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
}

const input = shuffled(N);
const runs = LANES.map((id) => {
  const alg = ALGORITHMS.find((a) => a.id === id);
  return { alg, log: record(alg, input) };
});
const maxSteps = Math.max(...runs.map((r) => r.log.length));
const total = RUN + HOLD;
const t = (step) => ((step / maxSteps) * RUN) / total; // step -> fraction of the loop
const f = (x) => x.toFixed(4).replace(/0+$/, "").replace(/\.$/, "") || "0";

const width = 880;
const cols = 3;
const laneW = 268;
const laneH = 150;
const gapX = (width - 32 - cols * laneW) / (cols - 1);
const top = 20;
const rows = Math.ceil(LANES.length / cols);
const height = top + rows * (laneH + 16) + 28;
const ranks = runs.map(
  (r) => 1 + runs.filter((o) => o.log.length < r.log.length).length,
);
const ORD = ["1st", "2nd", "3rd", "4th", "5th", "6th"];

let body = "";
runs.forEach((run, index) => {
  const x0 = 16 + (index % cols) * (laneW + gapX);
  const y0 = top + Math.floor(index / cols) * (laneH + 16);
  const plotX = x0 + 12;
  const plotW = laneW - 24;
  const plotY = y0 + 34;
  const plotH = laneH - 52;
  const slot = plotW / N;
  const finish = t(run.log.length);

  // Replay the log, collecting every change of every slot.
  const a = input.slice();
  const changes = a.map((v) => [[0, v]]);
  const cursor = [[0, 0]];
  run.log.forEach((e, s) => {
    const time = t(s + 1);
    if (e.op === MOVE) a[e.i] = e.to;
    else if (e.op === SWAP) [a[e.i], a[e.j]] = [a[e.j], a[e.i]];
    if (e.op === MOVE || e.op === SWAP) {
      changes[e.i].push([time, a[e.i]]);
      if (e.op === SWAP) changes[e.j].push([time, a[e.j]]);
    }
    if (cursor.at(-1)[1] !== e.i) cursor.push([time, e.i]);
  });

  body += `<rect x="${x0}" y="${y0}" width="${laneW}" height="${laneH}" fill="${PANEL}" stroke="${RULE}"/>`;
  body += `<text x="${x0 + 12}" y="${y0 + 22}" fill="${INK}" font-family="${FONT}" font-size="14" font-weight="700">${run.alg.name}</text>`;
  body += `<text x="${x0 + laneW - 12}" y="${y0 + 22}" text-anchor="end" fill="${DONE}" font-family="${FONT}" font-size="14" font-weight="800" opacity="0">${ORD[ranks[index] - 1]}`;
  body += `<animate attributeName="opacity" values="0;1" keyTimes="0;${f(finish)}" dur="${total}s" calcMode="discrete" repeatCount="indefinite"/></text>`;

  // Bars grow up from the baseline: flip the group so height is all that changes.
  body += `<g transform="translate(0 ${plotY + plotH}) scale(1 -1)" fill="${BAR}">`;
  body += `<animate attributeName="fill" values="${BAR};${DONE}" keyTimes="0;${f(finish)}" dur="${total}s" calcMode="discrete" repeatCount="indefinite"/>`;
  changes.forEach((list, k) => {
    const h = (v) => ((v / N) * plotH).toFixed(1);
    const xk = f(plotX + k * slot);
    const w = f(Math.max(1, slot - 1));
    if (list.length === 1) {
      body += `<rect x="${xk}" width="${w}" height="${h(list[0][1])}"/>`;
      return;
    }
    body += `<rect x="${xk}" width="${w}" height="${h(list[0][1])}"><animate attributeName="height" values="${list.map((c) => h(c[1])).join(";")}" keyTimes="${list.map((c) => f(c[0])).join(";")}" dur="${total}s" calcMode="discrete" repeatCount="indefinite"/></rect>`;
  });
  body += `</g>`;

  // A small marker under the element being worked on.
  body += `<rect y="${plotY + plotH + 4}" width="${f(Math.max(2, slot - 1))}" height="3" fill="${CURSOR}" x="${f(plotX)}">`;
  body += `<animate attributeName="x" values="${cursor.map((c) => f(plotX + c[1] * slot)).join(";")}" keyTimes="${cursor.map((c) => f(c[0])).join(";")}" dur="${total}s" calcMode="discrete" repeatCount="indefinite"/>`;
  body += `<animate attributeName="opacity" values="1;0" keyTimes="0;${f(finish)}" dur="${total}s" calcMode="discrete" repeatCount="indefinite"/></rect>`;
});

const caption = `The same ${N} numbers for each. One tick is one comparison or one moved element; the badge is the finishing place.`;
const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" role="img" aria-label="Animation: six sorting algorithms sorting the same array side by side">
<title>Six sorting algorithms racing on the same array</title>
<rect width="${width}" height="${height}" rx="6" fill="${BG}"/>
${body}
<text x="16" y="${height - 12}" fill="${MUTED}" font-family="${FONT}" font-size="12.5">${caption}</text>
</svg>
`;

mkdirSync(join(root, "docs", "assets"), { recursive: true });
writeFileSync(join(root, "docs", "assets", "race.svg"), svg);
console.log(
  `wrote docs/assets/race.svg (${(svg.length / 1024).toFixed(0)} KB)`,
);
for (const [i, r] of runs.entries())
  console.log(`  ${ORD[ranks[i] - 1]}  ${r.alg.name}: ${r.log.length} steps`);
