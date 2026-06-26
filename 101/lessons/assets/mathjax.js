// MathJax 3 config for Material + pymdownx.arithmatex(generic).
// arithmatex converts $...$ / $$...$$ to \(...\) / \[...\] at build time,
// so MathJax only needs the TeX-bracket delimiters. (Currency is written as
// "USD" in the lessons, so no $ ambiguity ever reaches the renderer.)
window.MathJax = {
  tex: {
    inlineMath: [["\\(", "\\)"]],
    displayMath: [["\\[", "\\]"]],
    processEscapes: true,
    processEnvironments: true,
  },
  options: {
    ignoreHtmlClass: ".*|",
    processHtmlClass: "arithmatex",
  },
};

// Re-typeset on Material's instant navigation.
document$.subscribe(() => {
  MathJax.startup.output.clearCache();
  MathJax.typesetClear();
  MathJax.texReset();
  MathJax.typesetPromise();
});
