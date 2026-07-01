/* Lesson widget mounter. A widget module calls CFWidgets.register(name, initFn)
   at load time; this scans the page once for `.lesson-widget[data-widget]`
   and calls the matching initFn(container). No instant-nav re-mount handling
   needed — this site does not use theme.features: navigation.instant. */
(function () {
  "use strict";

  var registry = {};

  window.CFWidgets = {
    register: function (name, initFn) {
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
      initFn(el);
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", mountAll);
  } else {
    mountAll();
  }
})();
