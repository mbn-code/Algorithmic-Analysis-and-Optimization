// Tests for the JavaScript ports in web/js/sorting.js, which drive the
// visualizer and the README animation. Run with: node --test "tests/*.test.mjs"

import { test } from "node:test";
import assert from "node:assert/strict";
import {
  ALGORITHMS,
  INPUTS,
  makeInput,
  record,
  Player,
  CMP,
} from "../web/js/sorting.js";

const SIZES = [1, 2, 3, 5, 31, 32, 33, 64, 128, 256];

for (const alg of ALGORITHMS) {
  test(`${alg.name} sorts every input`, () => {
    for (const n of SIZES) {
      for (const input of INPUTS) {
        const values = makeInput(input.id, n);
        const player = new Player(values, record(alg, values));
        player.seek(player.log.length);
        assert.deepEqual(
          player.a,
          values.slice().sort((a, b) => a - b),
          `${input.id}, n=${n}`,
        );
      }
    }
  });

  test(`${alg.name} replays backwards to the original input`, () => {
    const values = makeInput("random", 100);
    const player = new Player(values, record(alg, values));
    player.seek(player.log.length);
    player.seek(0);
    assert.deepEqual(player.a, values);
    assert.equal(player.comparisons, 0);
    assert.equal(player.moves, 0);
  });
}

test("the quicksort killer makes tuned quicksort do quadratic work, but not introsort", () => {
  const n = 512;
  const killer = makeInput("adversarial", n);
  const count = (id, values) =>
    record(
      ALGORITHMS.find((a) => a.id === id),
      values,
    ).filter((e) => e.op === CMP).length;
  const quick = count("quick", killer);
  const intro = count("intro", killer);
  assert.ok(quick > (n * n) / 16, `quick_sort made ${quick} comparisons`);
  assert.ok(
    intro < quick / 2,
    `intro_sort made ${intro} comparisons, quick_sort ${quick}`,
  );
});
