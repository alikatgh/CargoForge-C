# CargoForge-C — Feature Matrix

Tracking 100 features toward a world-class maritime stowage & stability tool.
`[x]` shipped · `[ ]` planned. Each shipped feature is covered by the test/CI gate.

## Placement & packing
1. [x] 2D shelf bin-packing heuristic
2. [x] Heaviest-first (First-Fit-Decreasing) ordering
3. [x] Item rotation (two orientations)
4. [x] Multi-bin stowage (holds + deck)
5. [x] Configurable hold count (`holds=`)
6. [x] CG-aware bin selection
7. [x] Transverse centering trim
8. [x] Both-axis bounds checking
9. [x] Oversize-cargo rejection
10. [ ] Per-hold weight limits
11. [ ] Vertical stacking / tiers
12. [ ] Hold depth modeling
13. [ ] Volume-utilization metric
14. [ ] Packing-efficiency report
15. [ ] Unplaced-cargo report section

## Stability & naval architecture
16. [x] Longitudinal center of gravity
17. [x] Transverse center of gravity
18. [x] Transverse metacentric height (GM)
19. [x] Overweight rejection
20. [x] Stability verdict (stable/unstable)
21. [x] Vertical CG (KG) reporting
22. [x] Center of buoyancy (KB) reporting
23. [x] Metacentric radius (BM) reporting
24. [x] Mean draft
25. [x] Draft fore / aft
26. [x] Trim
27. [x] Heel / list angle
28. [x] Longitudinal metacentric height (GML)
29. [x] Displacement
30. [x] Deadweight
31. [x] Displaced volume (m³)
32. [x] Freeboard (`depth_m`)
33. [ ] Load-line / Plimsoll check
34. [ ] Free-surface correction
35. [ ] Wind heeling moment
36. [ ] Righting arm (GZ) estimate
37. [ ] Angle of vanishing stability (approx)
38. [ ] IMO intact-stability criteria check (approx)
39. [ ] Ballast suggestion
40. [ ] Stability safety margin

## Cargo modeling & constraints
41. [x] Cargo type labels
42. [ ] Hazmat segregation rules
43. [ ] Stackability flags
44. [ ] Must-load / priority flags
45. [ ] Reefer power demand
46. [ ] Temperature zones
47. [ ] Max stack weight
48. [ ] Fragile-on-top rule
49. [ ] Out-of-gauge handling
50. [ ] Cargo grouping by destination
51. [ ] Load/discharge port sequencing
52. [ ] Weight-by-type summary
53. [x] Per-item CG contribution (`--verbose`)
54. [ ] Duplicate-ID detection
55. [ ] Manifest statistics
56. [ ] Heavy-item density warnings
57. [ ] Lashing/securing estimate
58. [ ] Dangerous-goods class labels

## Output & UX
59. [x] Human-readable plan
60. [x] JSON output (`--json`)
61. [ ] CSV output (`--csv`)
62. [x] Colored output (ANSI)
63. [x] `--color=auto|always|never`
64. [x] TTY auto-detection (+ NO_COLOR)
65. [ ] ASCII top-down ship diagram
66. [x] `--quiet` mode
67. [x] `--verbose` mode
68. [ ] Output sorting (`--sort`)
69. [ ] Box-drawing tables
70. [ ] Aligned tabular columns
71. [ ] Summary-only mode
72. [ ] Collected warnings summary
73. [x] Strict exit codes (`--strict`)
74. [ ] Progress / status messages
75. [ ] Imperial/metric unit toggle
76. [ ] Per-hold utilization bars
77. [ ] Stability gauge visualization
78. [ ] Markdown report (`--md`)

## Input & configuration
79. [x] `key=value` ship config
80. [x] Whitespace cargo manifest
81. [x] Comment lines
82. [x] Required-field validation
83. [x] Unknown-key warnings
84. [x] Over-long-line handling
85. [ ] CSV cargo input
86. [ ] JSON config input
87. [ ] stdin input (`-`)
88. [ ] `--init` config scaffold
89. [ ] Config echo / dump
90. [ ] Environment-variable overrides

## Engineering & tooling
91. [x] Unit tests (parser/placement/analysis)
92. [x] Sanitizers (ASan + UBSan)
93. [x] Static analysis (Clang analyzer)
94. [x] CI matrix (gcc + clang, `-Werror`)
95. [x] Parser fuzzer
96. [x] Man page
97. [x] `make install` / `uninstall`
98. [ ] Benchmark harness
99. [ ] Code-coverage reporting
100. [ ] Library mode + public API

---

**Shipped: 48 / 100.** Updated as features land.
