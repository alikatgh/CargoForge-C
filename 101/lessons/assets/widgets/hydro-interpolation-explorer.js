/* Lesson 35 — reverse hydrostatic lookup: given a displacement, find the two
   bracketing table rows and interpolate. Reuses the exact table from lesson
   20 (including its 8400 t worked example) for continuity across the course. */
(function () {
  "use strict";

  var TABLE = [
    { draft: 2.0, displ: 3200, kb: 1.06, bm: 12.1 },
    { draft: 4.0, displ: 6500, kb: 2.12, bm: 4.3 },
    { draft: 6.0, displ: 8000, kb: 3.18, bm: 2.05 },
    { draft: 8.0, displ: 11000, kb: 4.24, bm: 1.32 },
  ];

  function lerp(a, b, t) {
    return a + t * (b - a);
  }

  function bracket(v) {
    for (var i = 0; i < TABLE.length - 1; i++) {
      if (v >= TABLE[i].displ && v <= TABLE[i + 1].displ) return i;
    }
    return v < TABLE[0].displ ? 0 : TABLE.length - 2;
  }

  function init(container) {
    var minD = TABLE[0].displ,
      maxD = TABLE[TABLE.length - 1].displ;

    var rowsHtml = TABLE.map(function (r, i) {
      return (
        '<tr data-row="' +
        i +
        '"><td>' +
        r.draft.toFixed(1) +
        "</td><td>" +
        r.displ +
        "</td><td>" +
        r.kb.toFixed(2) +
        "</td><td>" +
        r.bm.toFixed(2) +
        "</td></tr>"
      );
    }).join("");

    container.innerHTML =
      '<div class="cf-widget">' +
      '<p class="cf-widget__hint">Try it — slide the target displacement and watch which two rows bracket it.</p>' +
      '<div class="cf-widget__controls">' +
      '<label class="cf-widget__control">Target displacement (t)' +
      '<input type="range" min="' +
      minD +
      '" max="' +
      maxD +
      '" step="10" value="8400" data-target>' +
      "<output data-target-out></output></label>" +
      "</div>" +
      '<table data-hydro-table style="width:100%;font-size:0.85em;margin-bottom:0.6rem">' +
      "<thead><tr><th>draft (m)</th><th>displ (t)</th><th>KB (m)</th><th>BM (m)</th></tr></thead>" +
      "<tbody>" +
      rowsHtml +
      "</tbody></table>" +
      '<svg class="cf-widget__svg" viewBox="0 0 600 60" role="img" xmlns="http://www.w3.org/2000/svg">' +
      "<title>Where the target displacement sits between the bracketing rows</title>" +
      '<line x1="40" y1="30" x2="560" y2="30" stroke="currentColor" stroke-opacity="0.4"></line>' +
      '<circle data-lo-dot cx="40" cy="30" r="4" fill="currentColor" opacity="0.6"></circle>' +
      '<circle data-hi-dot cx="560" cy="30" r="4" fill="currentColor" opacity="0.6"></circle>' +
      '<circle data-target-dot cy="30" r="6" fill="#12A594"></circle>' +
      "</svg>" +
      '<p class="cf-widget__readout">Interpolated draft = <strong data-draftvalue></strong> &nbsp;(t = <span data-tvalue></span>)</p>' +
      "</div>";

    var slider = container.querySelector("[data-target]");
    var out = container.querySelector("[data-target-out]");
    var rows = container.querySelectorAll("tr[data-row]");
    var targetDot = container.querySelector("[data-target-dot]");
    var draftValue = container.querySelector("[data-draftvalue]");
    var tValue = container.querySelector("[data-tvalue]");

    function render() {
      var v = parseFloat(slider.value);
      out.textContent = v.toFixed(0) + " t";

      var i = bracket(v);
      var lo = TABLE[i],
        hi = TABLE[i + 1];
      var t = (v - lo.displ) / (hi.displ - lo.displ);
      var draft = lerp(lo.draft, hi.draft, t);

      for (var r = 0; r < rows.length; r++) {
        rows[r].style.background =
          r === i || r === i + 1 ? "rgba(18,165,148,0.14)" : "";
      }

      var frac = (v - TABLE[0].displ) / (TABLE[TABLE.length - 1].displ - TABLE[0].displ);
      targetDot.setAttribute("cx", 40 + frac * (560 - 40));

      draftValue.textContent = draft.toFixed(2) + " m";
      tValue.textContent = t.toFixed(3);
    }

    slider.addEventListener("input", render);
    render();
  }

  window.CFWidgets.register("hydro-interpolation-explorer", init);
})();
