/* Lesson 38 — toggle AddressSanitizer on/off, then simulate reading freed
   memory. Off: silent garbage, program keeps running. On: instant abort at
   the exact line, dramatizing why CI runs the suite a second time under
   -fsanitize=address (make test-asan). */
(function () {
  "use strict";

  function randomGarbageHex() {
    var hex = "";
    for (var i = 0; i < 8; i++) hex += Math.floor(Math.random() * 16).toString(16);
    return "0x" + hex;
  }

  function init(container) {
    container.innerHTML =
      '<div class="cf-widget">' +
      '<p class="cf-widget__hint">Try it — simulate reading freed memory with the sanitizer off, then on.</p>' +
      '<div class="cf-widget__row">' +
      '<button type="button" class="cf-widget__button" data-toggle aria-pressed="false">Sanitizer: OFF</button>' +
      '<button type="button" class="cf-widget__button" data-simulate>Simulate: read freed memory</button>' +
      "</div>" +
      '<pre data-terminal style="background:var(--md-code-bg-color,rgba(0,0,0,0.04));border-radius:8px;padding:0.7rem 0.9rem;font-size:0.85em;min-height:2.4em;margin:0;white-space:pre-wrap"><code data-output>(no run yet)</code></pre>' +
      "</div>";

    var toggleBtn = container.querySelector("[data-toggle]");
    var simBtn = container.querySelector("[data-simulate]");
    var output = container.querySelector("[data-output]");

    var sanitizerOn = false;

    toggleBtn.addEventListener("click", function () {
      sanitizerOn = !sanitizerOn;
      toggleBtn.textContent = "Sanitizer: " + (sanitizerOn ? "ON" : "OFF");
      toggleBtn.setAttribute("aria-pressed", sanitizerOn ? "true" : "false");
    });

    simBtn.addEventListener("click", function () {
      if (sanitizerOn) {
        output.innerHTML =
          '<span style="color:#D05663">==12345==ERROR: AddressSanitizer: heap-use-after-free on address ' +
          randomGarbageHex() +
          "\n" +
          "READ of size 8 at " +
          randomGarbageHex() +
          " thread T0\n" +
          "    #0 in ship_cleanup src/parser.c:214\n" +
          "\nSUMMARY: AddressSanitizer: heap-use-after-free\n" +
          "== exit code 134 (aborted) ==</span>";
      } else {
        output.textContent =
          "read value = " +
          randomGarbageHex() +
          "  (garbage — no error, program keeps running)";
      }
    });
  }

  window.CFWidgets.register("asan-toggle-demo", init);
})();
