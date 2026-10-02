// The race: every algorithm sorts the same array, one tick per step of work.

import { ALGORITHMS, INPUTS, makeInput, record, Player } from "./sorting.js";
import { Bars } from "./bars.js";

// Speed slider positions, in steps per second (independent of refresh rate).
const SPEEDS = [60, 120, 180, 300, 480, 720, 1200, 1920, 3000, 4800, 7680];
const ORDINALS = ["1st", "2nd", "3rd", "4th", "5th", "6th", "7th", "8th"];
const number = new Intl.NumberFormat("en-US");

export function setupRace(root) {
  const inputSelect = root.querySelector("#race-input");
  const sizeInput = root.querySelector("#race-size");
  const sizeOut = root.querySelector("#race-size-out");
  const speedInput = root.querySelector("#race-speed");
  const runButton = root.querySelector("#race-run");
  const grid = root.querySelector("#lanes");
  const template = root.querySelector("#lane-template");

  for (const input of INPUTS) inputSelect.add(new Option(input.name, input.id));

  const lanes = ALGORITHMS.map((alg) => {
    const el = template.content.firstElementChild.cloneNode(true);
    el.querySelector(".lane-name").textContent = alg.name;
    el.querySelector(".lane-note").textContent = alg.note;
    grid.append(el);
    const lane = {
      alg,
      el,
      bars: new Bars(el.querySelector("canvas")),
      steps: el.querySelector(".lane-steps"),
      place: el.querySelector(".lane-place"),
      player: null,
      rank: 0,
    };
    lane.bars.onResize = () => lane.player && draw(lane);
    return lane;
  });

  let max = 1;
  let frame = 0;
  let visible = true;

  function draw(lane) {
    const p = lane.player;
    lane.bars.draw(p.a, max, p.current, p.done);
    lane.steps.textContent = `${number.format(p.pos)} steps`;
  }

  function finish(lane) {
    lane.el.classList.add("finished");
    lane.place.textContent = ORDINALS[lane.rank - 1];
    lane.place.hidden = false;
  }

  let budget = 0;
  let lastTime = 0;

  function tick(time) {
    frame = 0;
    if (!visible) {
      lastTime = 0;
      return;
    }
    const dt = lastTime ? Math.min(0.1, (time - lastTime) / 1000) : 1 / 60;
    lastTime = time;
    budget += SPEEDS[Number(speedInput.value)] * dt;
    const speed = Math.floor(budget);
    budget -= speed;
    let running = false;
    for (const lane of lanes) {
      if (lane.player.done) continue;
      lane.player.advance(speed);
      if (lane.player.done) finish(lane);
      else running = true;
      draw(lane);
    }
    runButton.textContent = running ? "Restart" : "Shuffle and race";
    if (running) frame = requestAnimationFrame(tick);
  }

  function start({ autoplay = true } = {}) {
    cancelAnimationFrame(frame);
    lastTime = 0;
    budget = 0;
    const values = makeInput(inputSelect.value, Number(sizeInput.value));
    max = Math.max(...values);
    for (const lane of lanes) {
      lane.player = new Player(values, record(lane.alg, values));
      lane.el.classList.remove("finished");
      lane.place.hidden = true;
    }
    // Rank by total work; equal work shares a place.
    for (const lane of lanes) {
      const total = lane.player.log.length;
      lane.rank =
        1 + lanes.filter((other) => other.player.log.length < total).length;
    }
    lanes.forEach(draw);
    if (autoplay) frame = requestAnimationFrame(tick);
    else runButton.textContent = "Start the race";
  }

  sizeInput.addEventListener("input", () => {
    sizeOut.value = sizeInput.value;
  });
  sizeInput.addEventListener("change", () => start());
  inputSelect.addEventListener("change", () => start());
  runButton.addEventListener("click", () => {
    const idle = lanes.every((lane) => lane.player.pos === 0);
    if (idle) frame = requestAnimationFrame(tick);
    else start();
  });

  // Pause while scrolled out of view, resume when back.
  new IntersectionObserver(([entry]) => {
    visible = entry.isIntersecting;
    if (
      visible &&
      !frame &&
      lanes.some((l) => l.player && l.player.pos > 0 && !l.player.done)
    ) {
      frame = requestAnimationFrame(tick);
    }
  }).observe(grid);

  sizeOut.value = sizeInput.value;
  const calm = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  start({ autoplay: !calm });
}
