/* Lesson 21 — live version of the static balance-beam SVG. Three weight
   sliders at fixed positions; the CG marker recomputes on every input via
   the same formula analysis.c uses: CG = Σ(weight_i * position_i) / Σ weight_i. */
(function () {
  "use strict";

  var POS = [2, 5, 8]; // fixed positions, 0-10 scale along the beam
  var DEFAULT_W = [1, 3, 2]; // matches the static diagram this widget sits below
  var BEAM_X0 = 70,
    BEAM_X1 = 530; // pixel span of the beam, viewBox 0 0 600 200
  var BLOCK_W = 50;

  function posToPx(p) {
    return BEAM_X0 + (p / 10) * (BEAM_X1 - BEAM_X0);
  }

  function init(container) {
    var blockPx = POS.map(posToPx);

    container.innerHTML =
      '<div class="cf-widget">' +
      '<p class="cf-widget__hint">Try it — drag each weight and watch the balance point move.</p>' +
      '<div class="cf-widget__controls">' +
      [0, 1, 2]
        .map(function (i) {
          var label = ["Weight A", "Weight B", "Weight C"][i];
          return (
            '<label class="cf-widget__control">' +
            label +
            '<input type="range" min="0" max="5" step="0.5" value="' +
            DEFAULT_W[i] +
            '" data-idx="' +
            i +
            '">' +
            '<output data-out="' +
            i +
            '"></output>' +
            "</label>"
          );
        })
        .join("") +
      "</div>" +
      '<svg class="cf-widget__svg" viewBox="0 0 600 200" role="img" xmlns="http://www.w3.org/2000/svg">' +
      "<title>Live centre-of-gravity balance beam</title>" +
      "<desc>Three sliders control the weight of three blocks at fixed positions on a beam; the marker below shows the live centre of gravity.</desc>" +
      '<rect x="' +
      BEAM_X0 +
      '" y="118" width="' +
      (BEAM_X1 - BEAM_X0) +
      '" height="8" rx="2" fill="currentColor" opacity="0.5"></rect>' +
      [0, 1, 2]
        .map(function (i) {
          return (
            '<rect data-block="' +
            i +
            '" x="' +
            (blockPx[i] - BLOCK_W / 2) +
            '" width="' +
            BLOCK_W +
            '" rx="2" fill="#12A594" fill-opacity="0.16" stroke="#12A594" stroke-width="1.1"></rect>'
          );
        })
        .join("") +
      '<path data-fulcrum fill="#D05663"></path>' +
      '<line data-cgline stroke="#D05663" stroke-width="1" stroke-dasharray="3 2" opacity="0.7"></line>' +
      '<text data-cglabel fill="#D05663" font-size="11" text-anchor="middle" font-weight="600" y="168">CG</text>' +
      "</svg>" +
      '<p class="cf-widget__readout">CG position = <strong data-cgvalue></strong> <span data-cgnote></span></p>' +
      "</div>";

    var sliders = container.querySelectorAll("input[type=range]");
    var outputs = container.querySelectorAll("output[data-out]");
    var blocks = container.querySelectorAll("rect[data-block]");
    var fulcrum = container.querySelector("[data-fulcrum]");
    var cgLine = container.querySelector("[data-cgline]");
    var cgLabel = container.querySelector("[data-cglabel]");
    var cgValue = container.querySelector("[data-cgvalue]");
    var cgNote = container.querySelector("[data-cgnote]");
    var readout = container.querySelector(".cf-widget__readout");

    function render() {
      var weights = [];
      for (var i = 0; i < sliders.length; i++) {
        weights[i] = parseFloat(sliders[i].value);
        outputs[i].textContent = weights[i].toFixed(1) + " t";
        var h = Math.max(4, weights[i] * 16);
        blocks[i].setAttribute("y", 118 - h);
        blocks[i].setAttribute("height", h);
      }

      var total = weights[0] + weights[1] + weights[2];
      var readoutWarn = "cf-widget__readout--warn";

      if (total <= 0.001) {
        fulcrum.setAttribute("opacity", "0.25");
        cgLine.setAttribute("opacity", "0.2");
        cgLabel.setAttribute("opacity", "0.3");
        cgValue.textContent = "—";
        cgNote.textContent = "(no weight loaded)";
        readout.classList.add(readoutWarn);
        return;
      }

      readout.classList.remove(readoutWarn);
      fulcrum.setAttribute("opacity", "1");
      cgLine.setAttribute("opacity", "0.7");
      cgLabel.setAttribute("opacity", "1");

      var cgPos =
        (weights[0] * POS[0] + weights[1] * POS[1] + weights[2] * POS[2]) /
        total;
      var cgPx = posToPx(cgPos);

      fulcrum.setAttribute(
        "d",
        "M" + cgPx + ",126 L" + (cgPx - 12) + ",150 L" + (cgPx + 12) + ",150 Z"
      );
      cgLine.setAttribute("x1", cgPx);
      cgLine.setAttribute("x2", cgPx);
      cgLine.setAttribute("y1", "118");
      cgLine.setAttribute("y2", "92");
      cgLabel.setAttribute("x", cgPx);

      cgValue.textContent = cgPos.toFixed(2) + " / 10";
      cgNote.textContent =
        "— " +
        (cgPos < 4.5
          ? "pulled toward Weight A"
          : cgPos > 5.5
          ? "pulled toward Weight C"
          : "roughly centred");
    }

    for (var i = 0; i < sliders.length; i++) {
      sliders[i].addEventListener("input", render);
    }
    render();
  }

  window.CFWidgets.register("cg-balance-beam", init);
})();
