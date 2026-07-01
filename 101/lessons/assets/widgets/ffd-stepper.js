/* Lesson 31 — First-Fit Decreasing. A fixed cargo list, already sorted
   largest-volume-first, is placed one item at a time into the first of 3
   bins with enough remaining weight capacity. Simplified to weight capacity
   only (the real placer in placement_3d.c also checks 3D space and tries
   all 6 orientations) — the widget's job is to teach the sort+first-fit
   strategy, not reimplement full 3D bin packing. */
(function () {
  "use strict";

  var ITEMS = [
    { name: "Reefer-A", vol: 60, weight: 12 },
    { name: "Container-B", vol: 48, weight: 10 },
    { name: "Machinery-C", vol: 36, weight: 15 },
    { name: "Container-D", vol: 30, weight: 8 },
    { name: "Crate-E", vol: 18, weight: 5 },
    { name: "Box-F", vol: 8, weight: 3 },
  ];
  var BINS = [
    { name: "ForwardHold", cap: 20 },
    { name: "AftHold", cap: 20 },
    { name: "Deck", cap: 25 },
  ];

  function init(container) {
    container.innerHTML =
      '<div class="cf-widget">' +
      '<p class="cf-widget__hint">Try it — step through placing the largest item first into the first bin that still has room.</p>' +
      '<div class="cf-widget__row">' +
      '<button type="button" class="cf-widget__button" data-step>Step → place next item</button>' +
      '<button type="button" class="cf-widget__button" data-reset>Reset</button>' +
      "</div>" +
      '<svg class="cf-widget__svg" viewBox="0 0 600 190" role="img" xmlns="http://www.w3.org/2000/svg">' +
      "<title>First-Fit Decreasing placement</title>" +
      "<desc>Three bins with weight capacity bars fill as cargo items are placed largest-first into the first bin with enough room.</desc>" +
      BINS.map(function (b, i) {
        var x = 30 + i * 190;
        return (
          '<text x="' +
          (x + 75) +
          '" y="18" font-size="11" text-anchor="middle" fill="currentColor" opacity="0.75">' +
          b.name +
          " (" +
          b.cap +
          "t)</text>" +
          '<rect x="' +
          x +
          '" y="28" width="150" height="26" rx="4" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.35"></rect>' +
          '<rect data-binfill="' +
          i +
          '" x="' +
          x +
          '" y="28" width="0" height="26" rx="4" fill="#12A594" fill-opacity="0.4"></rect>' +
          '<text data-binlabel="' +
          i +
          '" x="' +
          (x + 75) +
          '" y="46" font-size="10.5" text-anchor="middle" fill="currentColor">0 / ' +
          b.cap +
          "t</text>"
        );
      }).join("") +
      '<text x="300" y="76" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.6">sorted queue (largest volume first) →</text>' +
      '<g data-queue></g>' +
      "</svg>" +
      '<p class="cf-widget__readout" data-log>Ready — click Step to place <strong>' +
      ITEMS[0].name +
      "</strong> (" +
      ITEMS[0].weight +
      "t, largest volume).</p>" +
      "</div>";

    var stepBtn = container.querySelector("[data-step]");
    var resetBtn = container.querySelector("[data-reset]");
    var log = container.querySelector("[data-log]");
    var queueG = container.querySelector("[data-queue]");
    var binFills = container.querySelectorAll("[data-binfill]");
    var binLabels = container.querySelectorAll("[data-binlabel]");

    var used = [0, 0, 0];
    var next = 0;

    function drawQueue() {
      var html = "";
      for (var i = next; i < ITEMS.length; i++) {
        var x = 40 + (i - next) * 92;
        html +=
          '<rect x="' +
          x +
          '" y="88" width="82" height="30" rx="3" fill="' +
          (i === next ? "#12A594" : "currentColor") +
          '" fill-opacity="' +
          (i === next ? "0.18" : "0.04") +
          '" stroke="' +
          (i === next ? "#12A594" : "currentColor") +
          '" stroke-opacity="' +
          (i === next ? "0.7" : "0.3") +
          '"></rect>' +
          '<text x="' +
          (x + 41) +
          '" y="107" font-size="9" text-anchor="middle" fill="currentColor">' +
          ITEMS[i].name +
          "</text>";
      }
      queueG.innerHTML = html;
    }

    function render() {
      for (var i = 0; i < BINS.length; i++) {
        var pct = used[i] / BINS[i].cap;
        binFills[i].setAttribute("width", 150 * pct);
        binLabels[i].textContent = used[i] + " / " + BINS[i].cap + "t";
      }
      drawQueue();
    }

    stepBtn.addEventListener("click", function () {
      if (next >= ITEMS.length) return;
      var item = ITEMS[next];
      var placedIn = -1;
      for (var b = 0; b < BINS.length; b++) {
        if (used[b] + item.weight <= BINS[b].cap) {
          used[b] += item.weight;
          placedIn = b;
          break;
        }
      }
      if (placedIn === -1) {
        log.innerHTML =
          "<strong>" + item.name + "</strong> (" + item.weight + "t) — no bin has room. Unplaced (pos_x = -1).";
      } else {
        log.innerHTML =
          "<strong>" + item.name + "</strong> (" + item.weight + "t) → " + BINS[placedIn].name + ".";
      }
      next++;
      if (next >= ITEMS.length) {
        stepBtn.disabled = true;
        log.innerHTML += " <strong>All items placed.</strong>";
      } else {
        log.innerHTML += " Next: <strong>" + ITEMS[next].name + "</strong>.";
      }
      render();
    });

    resetBtn.addEventListener("click", function () {
      used = [0, 0, 0];
      next = 0;
      stepBtn.disabled = false;
      log.innerHTML =
        "Ready — click Step to place <strong>" +
        ITEMS[0].name +
        "</strong> (" +
        ITEMS[0].weight +
        "t, largest volume).";
      render();
    });

    render();
  }

  window.CFWidgets.register("ffd-stepper", init);
})();
