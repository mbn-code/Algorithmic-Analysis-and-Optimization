// Step through one algorithm, forwards or backwards, next to its C++ source.

import {
  ALGORITHMS,
  INPUTS,
  makeInput,
  record,
  Player,
  CMP,
  MOVE,
  SWAP,
  READ,
} from "./sorting.js";
import { Bars } from "./bars.js";

const number = new Intl.NumberFormat("en-US");

function narrate(e, a) {
  if (!e) return "Press play, or step forward one operation at a time.";
  switch (e.op) {
    case CMP:
      if (e.j >= 0)
        return `Compare a[${e.i}] = ${a[e.i]} with a[${e.j}] = ${a[e.j]}.`;
      if (e.from !== undefined)
        return `Compare ${e.from} with ${e.to} (position ${e.i}).`;
      return `Compare a[${e.i}] = ${a[e.i]}.`;
    case MOVE:
      return `Write ${e.to} to a[${e.i}].`;
    case SWAP:
      return `Swap a[${e.i}] and a[${e.j}].`;
    case READ:
      return `Read a[${e.i}] = ${a[e.i]}.`;
    default:
      return "";
  }
}

// A short blip whose pitch follows the value touched. Off until asked for.
class Blip {
  constructor() {
    this.ctx = null;
    this.last = 0;
  }
  play(fraction) {
    this.ctx ??= new AudioContext();
    const now = this.ctx.currentTime;
    if (now - this.last < 0.012) return;
    this.last = now;
    const osc = this.ctx.createOscillator();
    const gain = this.ctx.createGain();
    osc.type = "triangle";
    osc.frequency.value = 180 + 900 * fraction;
    gain.gain.setValueAtTime(0.06, now);
    gain.gain.exponentialRampToValueAtTime(0.0001, now + 0.08);
    osc.connect(gain).connect(this.ctx.destination);
    osc.start(now);
    osc.stop(now + 0.09);
  }
}

export function setupStudio(root) {
  const algSelect = root.querySelector("#studio-algorithm");
  const inputSelect = root.querySelector("#studio-input");
  const sizeSelect = root.querySelector("#studio-size");
  const playButton = root.querySelector("#studio-play");
  const backButton = root.querySelector("#studio-back");
  const nextButton = root.querySelector("#studio-next");
  const scrub = root.querySelector("#studio-scrub");
  const speedInput = root.querySelector("#studio-speed");
  const soundInput = root.querySelector("#studio-sound");
  const narration = root.querySelector("#studio-narration");
  const counters = root.querySelector("#studio-counters");
  const code = root.querySelector("#studio-code");
  const note = root.querySelector("#studio-note");
  const bars = new Bars(root.querySelector("#studio-canvas"));
  const blip = new Blip();

  for (const alg of ALGORITHMS) algSelect.add(new Option(alg.name, alg.id));
  for (const input of INPUTS) inputSelect.add(new Option(input.name, input.id));
  algSelect.value = "quick";

  let player = null;
  let values = [];
  let max = 1;
  let playing = false;
  let frame = 0;
  let lines = [];

  function render() {
    const e = player.current;
    bars.draw(player.a, max, e, player.done);
    narration.textContent = player.done ? "Sorted." : narrate(e, player.a);
    counters.textContent =
      `${number.format(player.comparisons)} comparisons, ${number.format(player.moves)} moves, ` +
      `step ${number.format(player.pos)} of ${number.format(player.log.length)}`;
    scrub.value = player.pos;
    const line = e && !player.done ? e.line : -1;
    lines.forEach((el, k) => el.classList.toggle("active", k === line));
    playButton.textContent = playing
      ? "Pause"
      : player.done
        ? "Replay"
        : "Play";
    playButton.setAttribute("aria-pressed", String(playing));
  }

  function load() {
    stop();
    const alg = ALGORITHMS.find((a) => a.id === algSelect.value);
    values = makeInput(inputSelect.value, Number(sizeSelect.value));
    max = Math.max(...values);
    player = new Player(values, record(alg, values));
    scrub.max = player.log.length;
    code.replaceChildren(
      ...alg.code.map((text) => {
        const span = document.createElement("span");
        span.className = "line";
        span.textContent = text || " ";
        return span;
      }),
    );
    lines = [...code.children];
    note.textContent = alg.note;
    render();
  }

  // Speed slider positions, in steps per second.
  const RATES = [2, 4, 8, 15, 30, 60, 120, 240, 480, 960, 1920];
  let budget = 0;
  let lastTime = 0;

  function tick(time) {
    const dt = lastTime ? Math.min(0.1, (time - lastTime) / 1000) : 1 / 60;
    lastTime = time;
    budget += RATES[Number(speedInput.value)] * dt;
    const steps = Math.floor(budget);
    budget -= steps;
    for (let s = 0; s < steps && !player.done; s++) player.forward();
    if (steps > 0 && soundInput.checked && player.current) {
      blip.play(player.a[player.current.i] / max);
    }
    if (player.done) playing = false;
    render();
    frame = playing ? requestAnimationFrame(tick) : 0;
  }

  function play() {
    if (player.done) player.seek(0);
    playing = true;
    budget = 1;
    lastTime = 0;
    frame = requestAnimationFrame(tick);
  }

  function stop() {
    playing = false;
    cancelAnimationFrame(frame);
    frame = 0;
  }

  function step(direction) {
    stop();
    if (direction > 0) player.forward();
    else player.backward();
    if (soundInput.checked && player.current)
      blip.play(player.a[player.current.i] / max);
    render();
  }

  playButton.addEventListener("click", () => {
    if (playing) stop();
    else play();
    render();
  });
  backButton.addEventListener("click", () => step(-1));
  nextButton.addEventListener("click", () => step(1));
  scrub.addEventListener("input", () => {
    stop();
    player.seek(Number(scrub.value));
    render();
  });
  for (const el of [algSelect, inputSelect, sizeSelect])
    el.addEventListener("change", load);
  bars.onResize = () => player && render();

  root.addEventListener("keydown", (event) => {
    if (event.target.matches("select, input[type=range]")) return;
    if (event.key === "ArrowRight") step(1);
    else if (event.key === "ArrowLeft") step(-1);
    else if (event.key === " " && event.target === root) {
      event.preventDefault();
      playButton.click();
    } else return;
  });

  load();
}
