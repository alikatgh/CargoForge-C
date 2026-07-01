/* Lesson 11 — step through malloc -> write -> free -> the fork in the road:
   read via the dangling pointer (undefined behaviour) vs. null it out first
   (safe). Mirrors the real fix in parse_cargo_list: free(), then set the
   pointer to NULL immediately. */
(function () {
  "use strict";

  var STAGES = ["initial", "allocated", "written", "freed", "uaf", "safe"];

  function init(container) {
    container.innerHTML =
      '<div class="cf-widget">' +
      '<p class="cf-widget__hint">Try it — step forward, then choose: read the freed block, or null the pointer out first.</p>' +
      '<div class="cf-widget__row">' +
      '<button type="button" class="cf-widget__button" data-step>Step →</button>' +
      '<button type="button" class="cf-widget__button" data-read style="display:none">Read via ptr (bad)</button>' +
      '<button type="button" class="cf-widget__button" data-null style="display:none">ptr = NULL (safe)</button>' +
      '<button type="button" class="cf-widget__button" data-reset>Reset</button>' +
      "</div>" +
      '<svg class="cf-widget__svg" viewBox="0 0 600 140" role="img" xmlns="http://www.w3.org/2000/svg">' +
      "<title>malloc/free lifecycle</title>" +
      "<desc>A pointer variable box on the left and a heap block on the right, showing the block's state and whether the pointer still references it.</desc>" +
      '<rect x="20" y="45" width="110" height="50" rx="5" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"></rect>' +
      '<text x="75" y="65" font-size="11" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">ptr</text>' +
      '<text data-ptrval x="75" y="82" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.7">NULL</text>' +
      '<path data-arrow d="" fill="none" stroke="#12A594" stroke-width="1.6" opacity="0"></path>' +
      '<rect data-block x="330" y="35" width="150" height="70" rx="5" fill="currentColor" fill-opacity="0.03" stroke="currentColor" stroke-opacity="0.3" stroke-dasharray="4 3"></rect>' +
      '<text data-blocklabel x="405" y="75" font-size="10.5" text-anchor="middle" fill="currentColor" opacity="0.5">(not yet allocated)</text>' +
      "</svg>" +
      '<p class="cf-widget__readout" data-log>ptr = NULL — nothing allocated yet.</p>' +
      "</div>";

    var stepBtn = container.querySelector("[data-step]");
    var readBtn = container.querySelector("[data-read]");
    var nullBtn = container.querySelector("[data-null]");
    var resetBtn = container.querySelector("[data-reset]");
    var ptrVal = container.querySelector("[data-ptrval]");
    var arrow = container.querySelector("[data-arrow]");
    var block = container.querySelector("[data-block]");
    var blockLabel = container.querySelector("[data-blocklabel]");
    var log = container.querySelector("[data-log]");
    var readout = container.querySelector(".cf-widget__readout");

    var stage = 0;

    function setBlock(fill, stroke, dash, label) {
      block.setAttribute("fill", fill);
      block.setAttribute("fill-opacity", "0.14");
      block.setAttribute("stroke", stroke);
      block.setAttribute("stroke-dasharray", dash);
      blockLabel.textContent = label;
    }

    function render() {
      readout.classList.remove("cf-widget__readout--warn");
      switch (STAGES[stage]) {
        case "initial":
          ptrVal.textContent = "NULL";
          arrow.setAttribute("opacity", "0");
          setBlock("currentColor", "currentColor", "4 3", "(not yet allocated)");
          block.setAttribute("fill-opacity", "0.03");
          block.setAttribute("stroke-opacity", "0.3");
          log.innerHTML = "ptr = NULL — nothing allocated yet.";
          stepBtn.style.display = "";
          readBtn.style.display = "none";
          nullBtn.style.display = "none";
          break;
        case "allocated":
          ptrVal.textContent = "0x7f2a10";
          arrow.setAttribute("d", "M130,70 L328,70");
          arrow.setAttribute("opacity", "0.8");
          setBlock("#12A594", "#12A594", "0");
          block.setAttribute("stroke-opacity", "0.7");
          blockLabel.textContent = "allocated (malloc)";
          log.innerHTML = "<strong>ptr = malloc(sizeof(Cargo))</strong> — heap block reserved.";
          break;
        case "written":
          blockLabel.textContent = "written: {id, weight, …}";
          log.innerHTML = "Data written through ptr.";
          break;
        case "freed":
          setBlock("currentColor", "currentColor", "4 3", "freed — memory reclaimed");
          block.setAttribute("fill-opacity", "0.06");
          block.setAttribute("stroke-opacity", "0.4");
          arrow.setAttribute("stroke", "#D05663");
          log.innerHTML =
            "<strong>free(ptr)</strong> — but ptr still holds 0x7f2a10. A <em>dangling pointer</em>. Now choose:";
          stepBtn.style.display = "none";
          readBtn.style.display = "";
          nullBtn.style.display = "";
          break;
        case "uaf":
          setBlock("#D05663", "#D05663", "0", "UAF! reading reclaimed memory");
          block.setAttribute("fill-opacity", "0.18");
          block.setAttribute("stroke-opacity", "0.9");
          log.innerHTML =
            "<strong>Heap-use-after-free.</strong> Undefined behaviour — could be garbage, could crash, could silently corrupt. This is the exact bug the fuzzer caught in parse_cargo_list.";
          readout.classList.add("cf-widget__readout--warn");
          readBtn.style.display = "none";
          nullBtn.style.display = "none";
          break;
        case "safe":
          ptrVal.textContent = "NULL";
          arrow.setAttribute("opacity", "0");
          log.innerHTML =
            "<strong>ptr = NULL</strong> — the dangling pointer is neutralized. Any future use is caught (NULL deref) instead of silently reading freed memory. This is the real fix in parse_cargo_list.";
          readBtn.style.display = "none";
          nullBtn.style.display = "none";
          break;
      }
    }

    stepBtn.addEventListener("click", function () {
      if (stage < 3) stage++;
      render();
    });
    readBtn.addEventListener("click", function () {
      stage = 4;
      render();
    });
    nullBtn.addEventListener("click", function () {
      stage = 5;
      render();
    });
    resetBtn.addEventListener("click", function () {
      stage = 0;
      stepBtn.disabled = false;
      render();
    });

    render();
  }

  window.CFWidgets.register("malloc-lifecycle-stepper", init);
})();
