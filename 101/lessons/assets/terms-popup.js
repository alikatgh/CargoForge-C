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
  var SVG_NS = "http://www.w3.org/2000/svg";

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
      if (el.namespaceURI === SVG_NS) return true; // never linkify inside inline SVG (title/desc/text/tspan/...)
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
        var before = pos > 0 ? text[pos - 1] : " ";
        if (/[A-Za-z0-9_]/.test(before)) {
          pos++;
          continue; // not a word-start position — skip the whole label scan
        }

        var matched = null;
        for (var m = 0; m < matchList.length; m++) {
          var label = matchList[m].label;
          if (
            (counts[matchList[m].entryIdx] || 0) >= MAX_PER_TERM
          )
            continue;
          if (text.substr(pos, label.length) !== label) continue;
          var after = pos + label.length < text.length ? text[pos + label.length] : " ";
          if (/[A-Za-z0-9_]/.test(after)) continue;
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

  // *emphasis* only — never inside a code span, so pointer syntax like
  // `Ship *ship` in backtick-quoted code is never mistaken for italics.
  var EMPHASIS_RE = /\*([^*\n]+)\*/g;

  function appendWithEmphasis(container, text) {
    EMPHASIS_RE.lastIndex = 0;
    var lastIndex = 0;
    var m;
    while ((m = EMPHASIS_RE.exec(text))) {
      if (m.index > lastIndex) container.appendChild(document.createTextNode(text.slice(lastIndex, m.index)));
      var em = document.createElement("em");
      em.textContent = m[1];
      container.appendChild(em);
      lastIndex = EMPHASIS_RE.lastIndex;
    }
    if (lastIndex < text.length) container.appendChild(document.createTextNode(text.slice(lastIndex)));
  }

  function renderRichText(container, text) {
    container.textContent = "";
    var parts = text.split("`");
    if (parts.length % 2 === 0) {
      // odd number of backticks (unmatched) — bail out to plain text
      container.textContent = text;
      return;
    }
    for (var i = 0; i < parts.length; i++) {
      if (parts[i] === "") continue;
      if (i % 2 === 1) {
        var code = document.createElement("code");
        code.textContent = parts[i];
        container.appendChild(code);
      } else {
        appendWithEmphasis(container, parts[i]);
      }
    }
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
      renderRichText(labelEl, entry.labels[0]);
      renderRichText(bodyEl, entry.body);
      pop.hidden = false;

      var r = btn.getBoundingClientRect();
      var popWidth = 300;
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
