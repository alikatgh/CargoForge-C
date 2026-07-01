/* Lesson 23 — GM = KB + BM - KG, plus the wall-sided GZ(theta) curve.
   Defaults reuse the worked example from lesson 21's console report
   (KG/KB/BM = 6.84/3.71/4.52 -> GM 1.39). Sliders move KG and BM; KB stays
   fixed (it is draft-driven, not the lesson's teaching point here). */
(function () {
  "use strict";

  var KB = 3.71;
  var MAX_ANGLE = 50; // wall-sided formula is taught as valid to ~40-50 deg
  var CHART_X0 = 60,
    CHART_X1 = 560; // pixel span for angle 0..MAX_ANGLE
  var BASE_Y = 210,
    GZ_SCALE = 55; // px per metre of GZ
  var GZ_MIN = -1.2,
    GZ_MAX = 2.2; // clamp range for the chart

  function angleToX(deg) {
    return CHART_X0 + (deg / MAX_ANGLE) * (CHART_X1 - CHART_X0);
  }
  function gzToY(gz) {
    var clamped = Math.max(GZ_MIN, Math.min(GZ_MAX, gz));
    return BASE_Y - clamped * GZ_SCALE;
  }
  function gzAt(deg, gm, bm) {
    var rad = (deg * Math.PI) / 180;
    return Math.sin(rad) * (gm + (bm * Math.pow(Math.tan(rad), 2)) / 2);
  }

  function init(container) {
    container.innerHTML =
      '<div class="cf-widget">' +
      '<p class="cf-widget__hint">Try it — raise KG (load cargo higher) or shrink BM (a narrower hull) and watch the righting-arm curve collapse.</p>' +
      '<div class="cf-widget__controls">' +
      '<label class="cf-widget__control">KG — height of cargo\'s centre of gravity' +
      '<input type="range" min="3" max="14" step="0.01" value="6.84" data-kg>' +
      "<output data-kg-out></output></label>" +
      '<label class="cf-widget__control">BM — metacentric radius (hull beam effect)' +
      '<input type="range" min="1" max="8" step="0.01" value="4.52" data-bm>' +
      "<output data-bm-out></output></label>" +
      "</div>" +
      '<svg class="cf-widget__svg" viewBox="0 0 600 240" role="img" xmlns="http://www.w3.org/2000/svg">' +
      "<title>Live GZ righting-arm curve</title>" +
      "<desc>A line chart of righting arm GZ against heel angle from 0 to 50 degrees, redrawn live as KG and BM sliders change GM.</desc>" +
      '<line data-baseline x1="' +
      CHART_X0 +
      '" x2="' +
      CHART_X1 +
      '" stroke="currentColor" stroke-opacity="0.35"></line>' +
      '<line data-imoline stroke="#D05663" stroke-dasharray="3 3" stroke-opacity="0.6"></line>' +
      '<text data-imolabel fill="#D05663" font-size="9.5" opacity="0.85">IMO min GZ@30° = 0.20 m</text>' +
      '<line data-refline x1="' +
      angleToX(30) +
      '" x2="' +
      angleToX(30) +
      '" stroke="currentColor" stroke-opacity="0.3" stroke-dasharray="2 2"></line>' +
      '<text x="' +
      angleToX(30) +
      '" y="' +
      (BASE_Y + 20) +
      '" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.6">30°</text>' +
      '<polyline data-curve fill="none" stroke="#12A594" stroke-width="2"></polyline>' +
      '<circle data-gz30dot r="4" fill="#12A594"></circle>' +
      "</svg>" +
      '<p class="cf-widget__readout">GM = <strong data-gmvalue></strong> &nbsp;·&nbsp; GZ@30° = <strong data-gz30value></strong></p>' +
      "</div>";

    var kgSlider = container.querySelector("[data-kg]");
    var bmSlider = container.querySelector("[data-bm]");
    var kgOut = container.querySelector("[data-kg-out]");
    var bmOut = container.querySelector("[data-bm-out]");
    var curve = container.querySelector("[data-curve]");
    var gz30dot = container.querySelector("[data-gz30dot]");
    var imoLine = container.querySelector("[data-imoline]");
    var imoLabel = container.querySelector("[data-imolabel]");
    var gmValue = container.querySelector("[data-gmvalue]");
    var gz30value = container.querySelector("[data-gz30value]");
    var readout = container.querySelector(".cf-widget__readout");

    var imoY = gzToY(0.2);
    imoLine.setAttribute("x1", CHART_X0);
    imoLine.setAttribute("x2", CHART_X1);
    imoLine.setAttribute("y1", imoY);
    imoLine.setAttribute("y2", imoY);
    imoLabel.setAttribute("x", CHART_X0 + 4);
    imoLabel.setAttribute("y", imoY - 5);

    function render() {
      var kg = parseFloat(kgSlider.value);
      var bm = parseFloat(bmSlider.value);
      kgOut.textContent = kg.toFixed(2) + " m";
      bmOut.textContent = bm.toFixed(2) + " m";

      var gm = KB + bm - kg;
      var points = [];
      for (var deg = 0; deg <= MAX_ANGLE; deg += 2) {
        points.push(angleToX(deg) + "," + gzToY(gzAt(deg, gm, bm)));
      }
      curve.setAttribute("points", points.join(" "));

      var gz30 = gzAt(30, gm, bm);
      gz30dot.setAttribute("cx", angleToX(30));
      gz30dot.setAttribute("cy", gzToY(gz30));

      gmValue.textContent = gm.toFixed(2) + " m";
      gz30value.textContent = gz30.toFixed(2) + " m";

      // Compare the same rounded value that's displayed, so a GZ@30 that
      // reads "0.20 m" is never classified as unsafe by float noise alone.
      var unsafe = gm <= 0 || parseFloat(gz30.toFixed(2)) < 0.2;
      readout.classList.toggle("cf-widget__readout--warn", unsafe);
      curve.setAttribute("stroke", gm <= 0 ? "#D05663" : "#12A594");
    }

    kgSlider.addEventListener("input", render);
    bmSlider.addEventListener("input", render);
    render();
  }

  window.CFWidgets.register("gm-stability-explorer", init);
})();
