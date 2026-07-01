/* Collapsible left (navigation) and right (table-of-contents) sidebars.
   Adds two icon buttons to the Material header; each hides/shows its sidebar
   and remembers the choice in localStorage. No template override needed. */
(function () {
  "use strict";
  var H = document.documentElement;
  var KEY = { nav: "sl-nav-hidden", toc: "sl-toc-hidden" };

  function isHidden(which) { return H.classList.contains(KEY[which]); }
  function set(which, hidden) {
    H.classList.toggle(KEY[which], hidden);
    try { localStorage.setItem(KEY[which], hidden ? "1" : "0"); } catch (e) {}
    syncButtons();
  }

  // Apply saved state as early as the script runs (minimise flash).
  try {
    if (localStorage.getItem(KEY.nav) === "1") H.classList.add(KEY.nav);
    if (localStorage.getItem(KEY.toc) === "1") H.classList.add(KEY.toc);
  } catch (e) {}

  var buttons = {};
  // A page frame with a solid bar on the left (nav) or right (toc). Inline
  // styles so Material's `fill: currentColor` icon rule can't fill the frame.
  var ICON = {
    nav: '<svg viewBox="0 0 24 24" aria-hidden="true"><rect x="3" y="4.5" width="18" height="15" rx="2.5" style="fill:none;stroke:currentColor;stroke-width:1.8"/><rect x="4.7" y="6.1" width="4.3" height="11.8" rx="1" style="fill:currentColor"/></svg>',
    toc: '<svg viewBox="0 0 24 24" aria-hidden="true"><rect x="3" y="4.5" width="18" height="15" rx="2.5" style="fill:none;stroke:currentColor;stroke-width:1.8"/><rect x="15" y="6.1" width="4.3" height="11.8" rx="1" style="fill:currentColor"/></svg>'
  };
  var LABEL = { nav: "navigation panel", toc: "contents panel" };

  function makeBtn(which) {
    var b = document.createElement("button");
    b.type = "button";
    b.className = "md-header__button md-icon sl-toggle sl-toggle--" + which;
    b.innerHTML = ICON[which];
    b.addEventListener("click", function () { set(which, !isHidden(which)); });
    buttons[which] = b;
    return b;
  }

  function syncButtons() {
    ["nav", "toc"].forEach(function (which) {
      var b = buttons[which];
      if (!b) return;
      var hidden = isHidden(which);
      b.setAttribute("aria-pressed", hidden ? "true" : "false");
      b.classList.toggle("sl-active", hidden);
      b.title = (hidden ? "Show" : "Hide") + " the " + LABEL[which];
      b.setAttribute("aria-label", b.title);
    });
  }

  function init() {
    var header = document.querySelector(".md-header__inner");
    if (!header || header.querySelector(".sl-toggle")) return;
    var navBtn = makeBtn("nav");
    var tocBtn = makeBtn("toc");

    // Left button: just left of the (flex-growing) title, so it sits on the left.
    var title = header.querySelector(".md-header__title");
    if (title) header.insertBefore(navBtn, title); else header.appendChild(navBtn);

    // Right button: just left of the palette / search cluster on the right.
    var opt = header.querySelector(".md-header__option, [data-md-component=palette], .md-search");
    if (opt) header.insertBefore(tocBtn, opt); else header.appendChild(tocBtn);

    syncButtons();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
