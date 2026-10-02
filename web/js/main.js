import { setupRace } from "./race.js";
import { setupStudio } from "./studio.js";
import { setupSearch } from "./search.js";
import { loadResults, lineChart } from "./charts.js";

const REPO = "mbn-code/Algorithmic-Analysis-and-Optimization";

setupRace(document.querySelector("#race"));
setupStudio(document.querySelector("#studio"));
setupSearch(document.querySelector("#search"));

loadResults()
  .then(({ sort, search, meta }) => {
    const machine = document.querySelector("#machine");
    if (meta.cpu) {
      machine.querySelector("[data-cpu]").textContent = meta.cpu;
      machine.querySelector("[data-compiler]").textContent =
        `${meta.compiler} on ${meta.os}`;
      machine.querySelector("[data-date]").textContent = meta.date;
      machine.hidden = false;
    }
    lineChart(document.querySelector("#chart-sort"), {
      rows: sort,
      metric: "ns_per_element",
      unit: "ns per element",
      dists: [
        "random",
        "sorted",
        "reversed",
        "nearly_sorted",
        "few_unique",
        "adversarial",
      ],
    });
    lineChart(document.querySelector("#chart-search"), {
      rows: search,
      metric: "median_ns_per_query",
      unit: "ns per lookup",
      dists: ["uniform", "skewed"],
      caches: meta.caches ?? [],
    });
  })
  .catch(() => {
    for (const holder of document.querySelectorAll(".chart-plot")) {
      holder.textContent =
        "The benchmark results did not load. They are in results/ in the repository.";
    }
  });

// Star count on the GitHub button, if the API answers.
fetch(`https://api.github.com/repos/${REPO}`)
  .then((r) => (r.ok ? r.json() : null))
  .then((repo) => {
    if (repo && repo.stargazers_count > 0) {
      const count = document.querySelector("#star-count");
      count.textContent = new Intl.NumberFormat("en-US").format(
        repo.stargazers_count,
      );
      count.hidden = false;
    }
  })
  .catch(() => {});
