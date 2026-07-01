/* Hover-glossary: walks rendered lesson text once, wraps the first couple of
   occurrences of each known glossary term in a clickable button, and shows
   its definition (from terms-data.js, extracted from glossary.md) in a small
   popover anchored near the trigger. Runs once per page load — this course
   does not use client-side instant navigation. */
(function () {
  "use strict";

  if (!window.CF_TERMS) return;

  var MAX_PER_TERM = 2;
  var SKIP_TAGS = { CODE: 1, PRE: 1, A: 1, H1: 1, H2: 1, H3: 1, H4: 1, H5: 1, H6: 1 };

  function escapeRegExp(s) {
    return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  }

  function buildMatchList() {
    var list = [];
    for (var i = 0; i < window.CF_TERMS.length; i++) {
      var labels = window.CF_TERMS[i].labels;
      for (var j = 0; j < labels.length; j++) {
        list.push({ label: labels[j], entryIdx: i });
      }
    }
    list.sort(function (a, b) {
      return b.label.length - a.label.length;
    });
    return list;
  }

  function isInsideSkippedTag(node) {
    var el = node.parentElement;
    while (el) {
      if (SKIP_TAGS[el.tagName]) return true;
      if (el.classList && el.classList.contains("glossary-term")) return true;
      el = el.parentElement;
    }
    return false;
  }

  function collectTextNodes(root) {
    var walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, null);
    var nodes = [];
    var node;
    while ((node = walker.nextNode())) {
      if (node.nodeValue.trim() && !isInsideSkippedTag(node)) nodes.push(node);
    }
    return nodes;
  }

  function linkify(root, matchList) {
    var counts = {};
    var nodes = collectTextNodes(root);

    nodes.forEach(function (textNode) {
      var text = textNode.nodeValue;
      var frag = null;
      var lastIndex = 0;

      for (var pos = 0; pos < text.length; ) {
        var matched = null;
        for (var m = 0; m < matchList.length; m++) {
          var label = matchList[m].label;
          if (
            (counts[matchList[m].entryIdx] || 0) >= MAX_PER_TERM
          )
            continue;
          if (text.substr(pos, label.length) !== label) continue;
          var before = pos > 0 ? text[pos - 1] : " ";
          var after = pos + label.length < text.length ? text[pos + label.length] : " ";
          if (/[A-Za-z0-9_]/.test(before) || /[A-Za-z0-9_]/.test(after)) continue;
          matched = matchList[m];
          break;
        }

        if (!matched) {
          pos++;
          continue;
        }

        if (!frag) frag = document.createDocumentFragment();
        if (pos > lastIndex) frag.appendChild(document.createTextNode(text.slice(lastIndex, pos)));

        var btn = document.createElement("button");
        btn.type = "button";
        btn.className = "glossary-term";
        btn.dataset.entryIdx = String(matched.entryIdx);
        btn.textContent = text.slice(pos, pos + matched.label.length);
        frag.appendChild(btn);

        counts[matched.entryIdx] = (counts[matched.entryIdx] || 0) + 1;
        pos += matched.label.length;
        lastIndex = pos;
      }

      if (frag) {
        if (lastIndex < text.length) frag.appendChild(document.createTextNode(text.slice(lastIndex)));
        textNode.parentNode.replaceChild(frag, textNode);
      }
    });
  }

  function setupPopover() {
    var pop = document.createElement("div");
    pop.className = "glossary-popover";
    pop.setAttribute("role", "tooltip");
    pop.hidden = true;
    pop.innerHTML =
      '<button type="button" class="glossary-popover__close" aria-label="Close">×</button>' +
      '<div class="glossary-popover__label"></div>' +
      '<div class="glossary-popover__body"></div>';
    document.body.appendChild(pop);

    var labelEl = pop.querySelector(".glossary-popover__label");
    var bodyEl = pop.querySelector(".glossary-popover__body");
    var closeBtn = pop.querySelector(".glossary-popover__close");

    function open(btn, entry) {
      labelEl.textContent = entry.labels[0];
      bodyEl.textContent = entry.body;
      pop.hidden = false;

      var r = btn.getBoundingClientRect();
      var popWidth = 320;
      var left = Math.min(Math.max(8, r.left), window.innerWidth - popWidth - 8);
      var top = r.bottom + 8;
      if (top + 160 > window.innerHeight) top = Math.max(8, r.top - 168);

      pop.style.left = left + "px";
      pop.style.top = top + "px";
    }

    function close() {
      pop.hidden = true;
    }

    document.addEventListener("click", function (e) {
      var btn = e.target.closest && e.target.closest(".glossary-term");
      if (btn) {
        var entry = window.CF_TERMS[parseInt(btn.dataset.entryIdx, 10)];
        if (entry) open(btn, entry);
        return;
      }
      if (!pop.hidden && !pop.contains(e.target)) close();
    });
    closeBtn.addEventListener("click", close);
    document.addEventListener("keydown", function (e) {
      if (e.key === "Escape") close();
    });
  }

  function run() {
    var root = document.querySelector(".md-typeset");
    if (!root) return;
    linkify(root, buildMatchList());
    setupPopover();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", run);
  } else {
    run();
  }
})();
