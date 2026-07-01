/* Lesson 27 — type a manifest line, watch it tokenize live the way
   parse_cargo_list does: split on whitespace (outer strtok_r), split the
   dimensions field on 'x' (inner strtok_r), and validate the weight field
   the way safe_atof does (reject anything that isn't a clean number). */
(function () {
  "use strict";

  var GOOD_LINE = "CRATE01 1200 2x3x1 standard";
  var BAD_LINE = "CRATE01 notanumber 2x3x1 standard";

  function safeAtofOk(s) {
    if (s === undefined) return false;
    return /^-?\d+(\.\d+)?$/.test(s.trim());
  }

  function init(container) {
    container.innerHTML =
      '<div class="cf-widget">' +
      '<p class="cf-widget__hint">Try it — edit the line below, or load a bad one, and watch each field get tokenized and validated.</p>' +
      '<div class="cf-widget__row">' +
      '<input type="text" data-line value="' +
      GOOD_LINE +
      '" style="flex:1 1 260px;font-family:var(--md-code-font,monospace);font-size:0.9em;padding:0.35rem 0.5rem;border:1px solid var(--md-default-fg-color--lightest);border-radius:6px;background:transparent;color:inherit">' +
      '<button type="button" class="cf-widget__button" data-bad>Try a bad line</button>' +
      '<button type="button" class="cf-widget__button" data-good>Reset</button>' +
      "</div>" +
      '<div class="cf-widget__controls" data-tokens></div>' +
      '<p class="cf-widget__readout" data-verdict></p>' +
      "</div>";

    var lineInput = container.querySelector("[data-line]");
    var badBtn = container.querySelector("[data-bad]");
    var goodBtn = container.querySelector("[data-good]");
    var tokensEl = container.querySelector("[data-tokens]");
    var verdict = container.querySelector("[data-verdict]");
    var readout = container.querySelector(".cf-widget__readout");

    function box(label, value, ok) {
      return (
        '<div class="cf-widget__control cf-widget__field' +
        (ok === false ? " cf-widget__field--invalid" : "") +
        '">' +
        '<span style="opacity:0.6;font-size:0.85em">' +
        label +
        "</span>" +
        '<span style="font-family:var(--md-code-font,monospace)">' +
        (value === "" ? "&mdash;" : value) +
        "</span>" +
        "</div>"
      );
    }

    function render() {
      var line = lineInput.value;
      var fields = line.trim().split(/\s+/);
      var id = fields[0] || "";
      var weight = fields[1] || "";
      var dims = fields[2] || "";
      var type = fields[3] || "";
      var dimParts = dims ? dims.split("x") : [];

      var weightOk = safeAtofOk(weight);

      var html = box("id", id) + box("weight (raw)", weight, weightOk);
      html += box(
        "dimensions",
        dimParts.length === 3 ? dimParts.join(" × ") : dims,
        dimParts.length === 3
      );
      html += box("type", type);
      tokensEl.innerHTML = html;

      if (!weight) {
        verdict.innerHTML = "Type a manifest line above.";
        readout.classList.remove("cf-widget__readout--warn");
      } else if (weightOk) {
        verdict.innerHTML =
          "safe_atof(\"" + weight + "\") → <strong>" + parseFloat(weight).toFixed(1) + "</strong> — accepted.";
        readout.classList.remove("cf-widget__readout--warn");
      } else {
        verdict.innerHTML =
          "safe_atof(\"" + weight + "\") → <strong>NAN</strong> — parse_cargo_list rejects this line and returns -1.";
        readout.classList.add("cf-widget__readout--warn");
      }
    }

    lineInput.addEventListener("input", render);
    badBtn.addEventListener("click", function () {
      lineInput.value = BAD_LINE;
      render();
    });
    goodBtn.addEventListener("click", function () {
      lineInput.value = GOOD_LINE;
      render();
    });

    render();
  }

  window.CFWidgets.register("tokenizer-playground", init);
})();
