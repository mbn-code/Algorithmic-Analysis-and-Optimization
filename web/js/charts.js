// Charts of the measured results in results/*.csv.
//
// Hue = algorithm family, dashed = the textbook version of that family, and
// the paper-white lines are the standard library for reference.

const SVG = "http://www.w3.org/2000/svg";

export const SERIES = {
  "std::sort": { color: "--paper", dash: "" },
  "std::stable_sort": { color: "--paper", dash: "6 4" },
  insertion_sort: { color: "--c4", dash: "" },
  "textbook::merge_sort": { color: "--c1", dash: "6 4" },
  merge_sort: { color: "--c1", dash: "" },
  "textbook::quick_sort": { color: "--c2", dash: "6 4" },
  quick_sort: { color: "--c2", dash: "" },
  intro_sort: { color: "--c2", dash: "1.5 3.5" },
  heap_sort: { color: "--c5", dash: "" },
  radix_sort: { color: "--c3", dash: "" },
  "std::lower_bound": { color: "--paper", dash: "" },
  "textbook::binary_search": { color: "--c1", dash: "6 4" },
  branchless_lower_bound: { color: "--c1", dash: "" },
  "textbook::interpolation_search": { color: "--c2", dash: "6 4" },
  interpolation_search: { color: "--c2", dash: "" },
  eytzinger_index: { color: "--c3", dash: "" },
};

const DIST_NAMES = {
  random: "Random",
  sorted: "Sorted",
  reversed: "Reversed",
  nearly_sorted: "Nearly sorted",
  few_unique: "Few unique",
  adversarial: "Quicksort killer",
  uniform: "Uniform keys",
  skewed: "Skewed keys",
};

const fmtN = (n) =>
  n >= 1 << 20 ? `${n / (1 << 20)}M` : n >= 1024 ? `${n / 1024}K` : String(n);
const fmtV = (v) =>
  v >= 100 ? v.toFixed(0) : v >= 10 ? v.toFixed(1) : v.toFixed(2);
const fmtBytes = (b) =>
  b >= 1 << 20 ? `${b / (1 << 20)} MB` : `${b / 1024} KB`;

function parseCsv(text) {
  const [head, ...rows] = text.trim().split(/\r?\n/);
  const keys = head.split(",");
  return rows.map((row) => {
    const cells = row.split(",");
    return Object.fromEntries(
      keys.map((k, i) => [
        k,
        Number.isNaN(Number(cells[i])) || cells[i] === ""
          ? cells[i]
          : Number(cells[i]),
      ]),
    );
  });
}

async function fetchFirst(paths) {
  for (const path of paths) {
    try {
      const response = await fetch(path);
      if (response.ok) return response;
    } catch {
      // try the next location
    }
  }
  throw new Error(`could not load ${paths[0]}`);
}

export async function loadResults() {
  const get = (name) => fetchFirst([`data/${name}`, `../results/${name}`]);
  const [sort, search, meta] = await Promise.all([
    get("sort.csv")
      .then((r) => r.text())
      .then(parseCsv),
    get("search.csv")
      .then((r) => r.text())
      .then(parseCsv),
    get("meta.json")
      .then((r) => r.json())
      .catch(() => ({})),
  ]);
  return { sort, search, meta };
}

function el(name, attrs = {}, parent) {
  const node = document.createElementNS(SVG, name);
  for (const [k, v] of Object.entries(attrs)) node.setAttribute(k, v);
  parent?.append(node);
  return node;
}

// One chart: picks a distribution, draws one line per algorithm.
export function lineChart(root, { rows, metric, unit, dists, caches = [] }) {
  const tabs = root.querySelector(".chart-tabs");
  const legend = root.querySelector(".chart-legend");
  const holder = root.querySelector(".chart-plot");
  const tip = root.querySelector(".chart-tip");
  const table = root.querySelector(".chart-table");

  const algorithms = [...new Set(rows.map((r) => r.algorithm))];
  const hidden = new Set();
  let dist = dists[0];
  let focus = null;

  for (const d of dists) {
    const b = document.createElement("button");
    b.type = "button";
    b.textContent = DIST_NAMES[d] ?? d;
    b.dataset.dist = d;
    b.addEventListener("click", () => {
      dist = d;
      render();
    });
    tabs.append(b);
  }

  for (const name of algorithms) {
    const s = SERIES[name] ?? { color: "--paper", dash: "" };
    const b = document.createElement("button");
    b.type = "button";
    b.className = "legend-item";
    b.setAttribute("aria-pressed", "true");
    const swatch = el("svg", { width: 28, height: 10, "aria-hidden": "true" });
    el(
      "line",
      {
        x1: 1,
        y1: 5,
        x2: 27,
        y2: 5,
        stroke: `var(${s.color})`,
        "stroke-width": 2,
        "stroke-dasharray": s.dash,
      },
      swatch,
    );
    b.append(swatch, name);
    b.addEventListener("click", () => {
      if (hidden.has(name)) hidden.delete(name);
      else hidden.add(name);
      b.setAttribute("aria-pressed", String(!hidden.has(name)));
      render();
    });
    b.addEventListener("pointerenter", () => {
      focus = name;
      render();
    });
    b.addEventListener("pointerleave", () => {
      focus = null;
      render();
    });
    legend.append(b);
  }

  function render() {
    for (const b of tabs.children)
      b.setAttribute("aria-pressed", String(b.dataset.dist === dist));
    const data = rows.filter((r) => r.distribution === dist);
    const sizes = [...new Set(data.map((r) => r.n))].sort((a, b) => a - b);
    const series = algorithms
      .filter((name) => !hidden.has(name))
      .map((name) => ({
        name,
        points: data
          .filter((r) => r.algorithm === name)
          .map((r) => [r.n, r[metric]]),
      }))
      .filter((s) => s.points.length);

    const width = holder.clientWidth || 640;
    const height = Math.max(260, Math.min(420, width * 0.55));
    const m = { top: 12, right: 16, bottom: 34, left: 52 };
    const w = width - m.left - m.right;
    const h = height - m.top - m.bottom;

    const values = series
      .flatMap((s) => s.points.map((p) => p[1]))
      .filter((v) => v > 0);
    const lo = Math.log10(Math.max(0.1, Math.min(...values, 1) * 0.8));
    const hi = Math.log10(Math.max(...values, 10) * 1.25);
    const nLo = Math.log2(sizes[0] ?? 1024);
    const nHi = Math.log2(sizes.at(-1) ?? 2048);
    const x = (n) =>
      m.left + ((Math.log2(n) - nLo) / Math.max(1, nHi - nLo)) * w;
    const y = (v) => m.top + (1 - (Math.log10(v) - lo) / (hi - lo)) * h;

    const svg = el("svg", {
      width,
      height,
      role: "img",
      "aria-label": `${unit} by input size, ${DIST_NAMES[dist]}`,
    });
    const grid = el("g", { class: "grid" }, svg);
    for (let e = Math.floor(lo); e <= Math.ceil(hi); e++) {
      for (const k of [1, 2, 5]) {
        const v = k * 10 ** e;
        if (Math.log10(v) < lo || Math.log10(v) > hi) continue;
        el("line", { x1: m.left, x2: m.left + w, y1: y(v), y2: y(v) }, grid);
        const t = el(
          "text",
          { x: m.left - 8, y: y(v) + 4, "text-anchor": "end" },
          grid,
        );
        t.textContent = v >= 1000 ? `${v / 1000}k` : String(v);
      }
    }
    const step = width < 520 ? 4 : 2;
    sizes.forEach((n, i) => {
      if (i % step && i !== sizes.length - 1) return;
      const t = el(
        "text",
        { x: x(n), y: m.top + h + 22, "text-anchor": "middle" },
        grid,
      );
      t.textContent = fmtN(n);
    });
    el(
      "line",
      {
        class: "axis",
        x1: m.left,
        x2: m.left + w,
        y1: m.top + h,
        y2: m.top + h,
      },
      svg,
    );

    for (const c of caches) {
      const n = c.bytes / 4;
      if (n <= sizes[0] || n >= sizes.at(-1)) continue;
      const g = el("g", { class: "cache" }, svg);
      el("line", { x1: x(n), x2: x(n), y1: m.top, y2: m.top + h }, g);
      const t = el("text", { x: x(n) + 4, y: m.top + 12 }, g);
      t.textContent = `${c.level} ${fmtBytes(c.bytes)}`;
    }

    for (const s of series) {
      const style = SERIES[s.name] ?? { color: "--paper", dash: "" };
      const d = s.points
        .map(
          ([n, v], i) =>
            `${i ? "L" : "M"}${x(n).toFixed(1)},${y(v).toFixed(1)}`,
        )
        .join("");
      el(
        "path",
        {
          d,
          fill: "none",
          stroke: `var(${style.color})`,
          "stroke-width": focus === s.name ? 3 : 2,
          "stroke-dasharray": style.dash,
          "stroke-linejoin": "round",
          opacity: focus && focus !== s.name ? 0.25 : 1,
        },
        svg,
      );
    }

    const cross = el(
      "line",
      { class: "crosshair", y1: m.top, y2: m.top + h, visibility: "hidden" },
      svg,
    );
    const hit = el(
      "rect",
      { x: m.left, y: m.top, width: w, height: h, fill: "transparent" },
      svg,
    );
    hit.addEventListener("pointermove", (event) => {
      const box = svg.getBoundingClientRect();
      const px = event.clientX - box.left;
      const n = sizes.reduce(
        (best, s) => (Math.abs(x(s) - px) < Math.abs(x(best) - px) ? s : best),
        sizes[0],
      );
      cross.setAttribute("x1", x(n));
      cross.setAttribute("x2", x(n));
      cross.setAttribute("visibility", "visible");
      const at = series
        .map((s) => ({
          name: s.name,
          v: s.points.find((p) => p[0] === n)?.[1],
        }))
        .filter((r) => r.v !== undefined)
        .sort((a, b) => a.v - b.v);
      tip.replaceChildren();
      const head = document.createElement("strong");
      head.textContent = `n = ${fmtN(n)}`;
      tip.append(head);
      for (const r of at) {
        const row = document.createElement("div");
        const sw = document.createElement("span");
        sw.className = "tip-swatch";
        sw.style.background = `var(${(SERIES[r.name] ?? { color: "--paper" }).color})`;
        const label = document.createElement("span");
        label.textContent = r.name;
        const value = document.createElement("span");
        value.className = "tip-value";
        value.textContent = `${fmtV(r.v)} ${unit}`;
        row.append(sw, label, value);
        tip.append(row);
      }
      tip.hidden = false;
      const left = x(n) + 14;
      tip.style.left = `${Math.min(left, width - tip.offsetWidth - 4)}px`;
      tip.style.top = `${m.top}px`;
      if (left + tip.offsetWidth > width - 4)
        tip.style.left = `${Math.max(0, x(n) - tip.offsetWidth - 14)}px`;
    });
    hit.addEventListener("pointerleave", () => {
      cross.setAttribute("visibility", "hidden");
      tip.hidden = true;
    });

    holder.replaceChildren(svg);
    renderTable(data, sizes);
  }

  function renderTable(data, sizes) {
    const head = `<tr><th scope="col">Algorithm</th>${sizes.map((n) => `<th scope="col">${fmtN(n)}</th>`).join("")}</tr>`;
    const body = algorithms
      .map((name) => {
        const cells = sizes.map((n) => {
          const r = data.find((d) => d.algorithm === name && d.n === n);
          return `<td>${r ? fmtV(r[metric]) : ""}</td>`;
        });
        return `<tr><th scope="row">${name}</th>${cells.join("")}</tr>`;
      })
      .join("");
    table.innerHTML = `<caption>${unit}, ${DIST_NAMES[dist]}</caption><thead>${head}</thead><tbody>${body}</tbody>`;
  }

  // Re-render on width changes only: the chart sets its own height.
  let lastWidth = 0;
  new ResizeObserver(() => {
    if (holder.clientWidth === lastWidth) return;
    lastWidth = holder.clientWidth;
    render();
  }).observe(holder);
  render();
}
