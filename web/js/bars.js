// Draws an array as bars on a canvas, highlighting the last operation.

import { CMP, MOVE, SWAP, READ } from "./sorting.js";

const css = (name) =>
  getComputedStyle(document.documentElement).getPropertyValue(name).trim();

let palette = null;
function colors() {
  palette ??= {
    bar: css("--bar"),
    range: css("--range"),
    compare: css("--compare"),
    move: css("--move"),
    read: css("--read"),
    done: css("--done"),
  };
  return palette;
}

export class Bars {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");
    this.width = 0;
    this.height = 0;
    new ResizeObserver(() => this.resize()).observe(canvas);
    this.resize();
  }

  resize() {
    const dpr = window.devicePixelRatio || 1;
    const { width, height } = this.canvas.getBoundingClientRect();
    if (width === 0 || height === 0) return;
    this.width = width;
    this.height = height;
    this.canvas.width = Math.round(width * dpr);
    this.canvas.height = Math.round(height * dpr);
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    this.onResize?.();
  }

  // values: the array; max: the largest value; event: last log entry or null.
  draw(values, max, event, finished = false) {
    const { ctx, width, height } = this;
    if (!width) return;
    const c = colors();
    const n = values.length;
    const slot = width / n;
    const gap = slot > 4 ? 1 : 0;
    ctx.clearRect(0, 0, width, height);

    if (event && !finished && event.hi - event.lo < n) {
      ctx.fillStyle = c.range;
      ctx.fillRect(event.lo * slot, 0, (event.hi - event.lo) * slot, height);
    }

    const hot = new Map();
    if (event && !finished) {
      const color =
        event.op === CMP ? c.compare : event.op === READ ? c.read : c.move;
      hot.set(event.i, color);
      if (event.j >= 0 && (event.op === CMP || event.op === SWAP))
        hot.set(event.j, color);
    }

    for (let k = 0; k < n; k++) {
      const h = Math.max(1, (values[k] / max) * (height - 2));
      ctx.fillStyle = finished ? c.done : (hot.get(k) ?? c.bar);
      ctx.fillRect(k * slot, height - h, slot - gap, h);
    }
  }
}

