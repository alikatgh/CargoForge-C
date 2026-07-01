/* Lesson widget mounter. A widget module calls CFWidgets.register(name, initFn)
   at load time; this scans the page once for `.lesson-widget[data-widget]`
   and calls the matching initFn(container). No instant-nav re-mount handling
   needed — this site does not use theme.features: navigation.instant. */
(function () {
  "use strict";

  var registry = {};

  window.CFWidgets = {
    register: function (name, initFn) {
      if (registry[name]) {
        console.warn('[cf-widgets] "' + name + '" is already registered — overwriting.');
      }
      registry[name] = initFn;
    },
  };

  function mountAll() {
    var nodes = document.querySelectorAll(".lesson-widget[data-widget]");
    for (var i = 0; i < nodes.length; i++) {
      var el = nodes[i];
      if (el.dataset.cfMounted) continue;
      var name = el.dataset.widget;
      var initFn = registry[name];
      if (!initFn) {
        console.warn('[cf-widgets] no widget registered for "' + name + '"');
        continue;
      }
      el.dataset.cfMounted = "true";
      try {
        initFn(el);
      } catch (e) {
        console.error('[cf-widgets] widget "' + name + '" failed to mount:', e);
      }
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", mountAll);
  } else {
    mountAll();
  }
})();
