/* Lesson 36 — free-surface moment FSM = rho*l*b^3/12. The formula has no
   fill-fraction term: it is ~constant for any partial fill and only drops
   to zero right at the empty/full extremes (matches the lesson's own text:
   "its fill level barely at all"; do NOT show a hump peaking at 50%). A
   lengthwise baffle halves breadth, cutting total FSM to ~1/4. */
(function () {
  "use strict";

  var RHO = 1.025; // seawater, t/m3
  var L = 10,
    B = 14; // tank length/breadth, m
  var DISPLACEMENT = 8000; // t
  var GM_BASE = 1.39; // matches the lesson 21 worked example

  function edgeRamp(fillPct) {
    // 0 at the exact extremes, full strength for 3%..97%, smooth ramp between.
    if (fillPct <= 0 || fillPct >= 100) return 0;
    if (fillPct < 3) return fillPct / 3;
    if (fillPct > 97) return (100 - fillPct) / 3;
    return 1;
  }

  function init(container) {
    container.innerHTML =
      '<div class="cf-widget">' +
      '<p class="cf-widget__hint">Try it — drag the fill level from empty to full, then toggle a baffle. Notice the penalty is at full strength across almost the whole range.</p>' +
      '<div class="cf-widget__controls">' +
      '<label class="cf-widget__control">Tank fill level' +
      '<input type="range" min="0" max="100" step="1" value="50" data-fill>' +
      "<output data-fill-out></output></label>" +
      "</div>" +
      '<div class="cf-widget__row">' +
      '<button type="button" class="cf-widget__button" data-baffle aria-pressed="false">+ Add lengthwise baffle</button>' +
      "</div>" +
      '<svg class="cf-widget__svg" viewBox="0 0 600 150" role="img" xmlns="http://www.w3.org/2000/svg">' +
      "<title>Tank cross-section and free-surface penalty gauge</title>" +
      "<desc>A tank cross-section fills to the chosen level; a horizontal gauge below shows how much the free-surface effect reduces GM.</desc>" +
      '<rect x="40" y="20" width="140" height="90" fill="none" stroke="currentColor" stroke-opacity="0.6" stroke-width="1.3"></rect>' +
      '<rect data-liquid x="40" width="140" fill="#12A594" fill-opacity="0.22"></rect>' +
      '<line data-baffleline x1="110" y1="20" x2="110" y2="110" stroke="currentColor" stroke-width="2" stroke-opacity="0" ></line>' +
      '<text x="110" y="128" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.6">tank cross-section</text>' +
      '<rect x="260" y="55" width="300" height="18" rx="3" fill="currentColor" fill-opacity="0.06" stroke="currentColor" stroke-opacity="0.3"></rect>' +
      '<rect data-gauge x="260" y="55" height="18" rx="3" fill="#D05663" fill-opacity="0.55"></rect>' +
      '<text x="260" y="45" font-size="10" fill="currentColor" opacity="0.7">GM penalty (virtual KG rise)</text>' +
      "</svg>" +
      '<p class="cf-widget__readout">GM: <strong data-gmbase></strong> corrected to <strong data-gmcorrected></strong> <span data-gmnote></span></p>' +
      "</div>";

    var fillSlider = container.querySelector("[data-fill]");
    var fillOut = container.querySelector("[data-fill-out]");
    var baffleBtn = container.querySelector("[data-baffle]");
    var liquid = container.querySelector("[data-liquid]");
    var baffleLine = container.querySelector("[data-baffleline]");
    var gauge = container.querySelector("[data-gauge]");
    var gmBase = container.querySelector("[data-gmbase]");
    var gmCorrected = container.querySelector("[data-gmcorrected]");
    var gmNote = container.querySelector("[data-gmnote]");
    var readout = container.querySelector(".cf-widget__readout");

    var baffled = false;
    var fsmFull = (RHO * L * Math.pow(B, 3)) / 12; // full-strength FSM, tonne-metres

    function render() {
      var fill = parseFloat(fillSlider.value);
      fillOut.textContent = fill.toFixed(0) + " %";

      var tankH = 90;
      var liquidH = (fill / 100) * tankH;
      liquid.setAttribute("y", 110 - liquidH);
      liquid.setAttribute("height", liquidH);

      baffleLine.setAttribute("stroke-opacity", baffled ? "0.7" : "0");
      baffleBtn.setAttribute("aria-pressed", baffled ? "true" : "false");
      baffleBtn.textContent = baffled
        ? "✓ Lengthwise baffle fitted"
        : "+ Add lengthwise baffle";

      var fsm = fsmFull * edgeRamp(fill) * (baffled ? 0.25 : 1);
      var fsc = fsm / DISPLACEMENT;
      var gmCorr = GM_BASE - fsc;

      var maxFsc = fsmFull / DISPLACEMENT;
      var gaugeW = Math.min(300, (fsc / maxFsc) * 300);
      gauge.setAttribute("width", gaugeW);

      gmBase.textContent = GM_BASE.toFixed(2) + " m";
      gmCorrected.textContent = gmCorr.toFixed(2) + " m";
      gmNote.textContent =
        "— free-surface penalty " + fsc.toFixed(3) + " m" + (baffled ? " (baffled)" : "");

      var severe = fsc > maxFsc * 0.5;
      readout.classList.toggle("cf-widget__readout--warn", severe);
    }

    fillSlider.addEventListener("input", render);
    baffleBtn.addEventListener("click", function () {
      baffled = !baffled;
      render();
    });
    render();
  }

  window.CFWidgets.register("free-surface-explorer", init);
})();
