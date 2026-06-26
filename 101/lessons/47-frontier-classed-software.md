# Frontier: classed stability software

CargoForge-C teaches the physics and the code — but it is a teaching tool, not a class-society-approved loading computer. This lesson maps the gap: what commercial systems like NAPA and GHS add on top of what you now understand, where the simplifications in CargoForge-C live, and how a learner or developer crosses from "interesting open source" to "software that shipping companies are allowed to use."

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Type approval** = "an official certificate from a classification society that says this software is allowed to be used on real ships" — without it, a shipping company's insurer won't cover losses and port state control can detain the vessel, regardless of how correct the arithmetic inside the software is.
- **Wall-sided GZ formula** = "an equation in `analysis.c` (`gz_at_angle`) that estimates how strongly a ship rights itself when heeled, assuming the hull sides are perfectly vertical at the waterline" — it works reasonably well at small angles but grows increasingly wrong above about 30° because real hulls flare outward, so the formula understates the righting lever.
- **Box-hull fallback** = "when no hydrostatic table CSV is provided, `perform_analysis` in `analysis.c` assumes the ship is shaped like a rectangular box, using fixed constants like $C_b = 0.75$ and $KB = 0.53 \times T$" — these constants are teaching approximations that are systematically wrong for almost every real hull form, which is why table-interpolation mode (`hydrostatic_table=` in the ship config) exists.
- **Linear interpolation (`lerp`)** = "in `hydrostatics.c`, when the current draft falls between two CSV rows, CargoForge-C draws a straight line between them to estimate displacement and other values" — real stability booklets use curved (higher-order) interpolation because a hull widens non-linearly as it rises out of the water, so linear interpolation introduces small but predictable errors near the bilge.
- **20-station longitudinal model** = "in `longitudinal_strength.c`, the ship's length is divided into 20 slices to compute shear force and bending moment along the hull, using a simple three-zone trapezoidal guess for where the lightship weight sits" — class software uses hundreds of stations tied to actual construction drawings; CargoForge-C's output is useful for gross checks but would not satisfy class documentation.
- **`imo_compliant` flag** = "the boolean in `AnalysisResult` that records whether the ship passes the six intact stability criteria from IMO MSC.267(85) §2.2" — it covers only the basic intact range up to 40°; it does not check damage stability, the weather criterion, or the newer dynamic stability criteria that class software handles as separate licensed modules.

**Why it matters:** Shipping companies are legally required to use approved loading computers; a program with correct physics but no type-approval certificate cannot be used operationally, no matter how good its code is. Understanding where CargoForge-C deliberately simplifies — box-hull KB, wall-sided GZ, linear interpolation, 20 stations — tells you exactly what would need to be replaced on the path to a real DNV-SE-0475 submission.

---

## The mental model 🧠

You'll forget the formula — hold THIS picture instead:

> A home mechanic can tune an engine beautifully, but that same engine cannot be bolted into a commercial aircraft until an aviation authority stamps every weld and every calculation. The mechanic's physics are correct. The stamp is not about correctness — it is about *documented, verified, insured correctness*.

CargoForge-C is the skilled mechanic. Its `gz_at_angle` formula in `analysis.c` is sound for small heels; its `lerp` in `hydrostatics.c` genuinely tracks displacement through the CSV rows; the `imo_compliant` flag really does check the six MSC.267(85) §2.2 criteria. The physics are right. What is missing is the aviation-authority stamp — the benchmark runs against three vessel types, the technical manual citing every IMO source, the >95% test coverage demanded by DNV-SE-0475.

The box-hull fallback ($C_b = 0.75$, $KB = 0.53T$) and the wall-sided GZ are not bugs — they are the mechanic choosing a workbench approximation over a certified instrument. Fine for teaching; grounded in the chapter.

<svg viewBox="0 0 620 210" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>CargoForge-C vs class-approved software gap diagram</title>
  <desc>Two boxes side by side. Left box labelled CargoForge-C lists its core modules: gz_at_angle (wall-sided), lerp hydrostatics, 20 stations, box-hull KB. Right box labelled Class-Approved (e.g. NAPA / GHS) lists NURBS hull geometry, cubic splines, hundreds of stations, damage stability, weather criterion. An arrow from left to right is labelled DNV-SE-0475 approval path (benchmarks, technical manual, 95% coverage).</desc>
  <!-- left box: CargoForge-C -->
  <rect x="10" y="20" width="240" height="170" rx="6" fill="none" stroke="currentColor" stroke-width="1.5"/>
  <text x="130" y="44" text-anchor="middle" font-size="13" font-weight="700" fill="currentColor">CargoForge-C</text>
  <line x1="10" y1="52" x2="250" y2="52" stroke="currentColor" stroke-width="1" opacity="0.35"/>
  <text x="24" y="72" font-size="11" fill="currentColor">gz_at_angle — wall-sided formula</text>
  <text x="24" y="90" font-size="11" fill="currentColor">lerp — linear hydrostatic interpolation</text>
  <text x="24" y="108" font-size="11" fill="currentColor">20 stations, trapezoidal lightship</text>
  <text x="24" y="126" font-size="11" fill="currentColor">box-hull fallback (C&#x1D47; = 0.75)</text>
  <text x="24" y="144" font-size="11" fill="currentColor">imo_compliant — §2.2 only (0–40°)</text>
  <text x="24" y="168" font-size="11.5" font-weight="600" fill="#D05663">No type-approval certificate</text>
  <!-- right box: Class-approved -->
  <rect x="370" y="20" width="240" height="170" rx="6" fill="none" stroke="#12A594" stroke-width="1.8"/>
  <text x="490" y="44" text-anchor="middle" font-size="13" font-weight="700" fill="#12A594">Class-Approved (NAPA / GHS)</text>
  <line x1="370" y1="52" x2="610" y2="52" stroke="#12A594" stroke-width="1" opacity="0.4"/>
  <text x="384" y="72" font-size="11" fill="currentColor">NURBS / mesh hull geometry</text>
  <text x="384" y="90" font-size="11" fill="currentColor">Cubic-spline hydrostatics</text>
  <text x="384" y="108" font-size="11" fill="currentColor">Hundreds of stations (frame data)</text>
  <text x="384" y="126" font-size="11" fill="currentColor">Damage stability + weather criterion</text>
  <text x="384" y="144" font-size="11" fill="currentColor">Trimmed hydrostatics (bilinear)</text>
  <text x="384" y="168" font-size="11.5" font-weight="600" fill="#12A594">DNV-SE-0475 type-approved</text>
  <!-- arrow -->
  <line x1="258" y1="105" x2="362" y2="105" stroke="currentColor" stroke-width="1.5" stroke-dasharray="5,3" opacity="0.6"/>
  <polygon points="362,100 374,105 362,110" fill="currentColor" opacity="0.6"/>
  <text x="311" y="96" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">approval path</text>
  <text x="311" y="119" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">benchmarks + manual</text>
  <text x="311" y="133" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">+ 95% coverage</text>
</svg>

## What "classed" means and why it matters

A vessel cannot sail under an approved flag without a valid stability instrument — software or hardware — that a classification society has type-approved. The five societies that control most of the world fleet are DNV (Norway), Lloyd's Register (UK), Bureau Veritas (France), ClassNK (Japan), and ABS (USA). Their type-approval programs for software require:

- A mathematical description of every calculation method, citing the IMO instruments (SOLAS, MSC.267(85), etc.)
- Validation against at least three benchmark vessels with known stability booklets
- Comparison against already-approved software (DNV's program is DNV-SE-0475)
- A regression test suite with high code coverage and documented change management

Without that certificate, the P&I insurer will not cover losses, port state control can detain the vessel, and the flag state can revoke the ship's certificates. No approved certificate = the software cannot be used operationally, regardless of how correct its arithmetic is.

The ROADMAP.md is explicit:

> "A shipping company cannot use unapproved software for stability calculations — their P&I insurance won't cover losses, port state control will detain the vessel."

---

## What commercial systems add

### Hull geometry and cross-curves

CargoForge-C bypasses hull geometry entirely. Its two hydrostatic modes are:

| Mode | Trigger | What it does |
|---|---|---|
| Box-hull approximation | No `hydrostatic_table=` in ship config | Uses $C_b = 0.75$, $C_{wp} = 0.85$, $KB = 0.53 \times T$ — constants that are wrong for almost every real hull |
| Table interpolation | `hydrostatic_table=` points to a CSV | Linear interpolation over the shipyard's own numbers |

Even in table mode, CargoForge-C interpolates from an even-keel table. Systems like NAPA work from the actual 3D hull surface — a NURBS or polyhedral mesh — and compute hydrostatics at any combination of trim and heel directly. This matters because a trimmed ship is not on an even keel, and the GM at $5°$ trim can differ meaningfully from the even-keel value.

NAPA additionally generates **cross-curves of stability** (GZ versus heel at constant displacement for a range of displacements). These are the curves in a real stability booklet. CargoForge-C's GZ calculation uses the wall-sided formula from `analysis.c`:

$$GZ(\theta) = \sin\theta \left( GM + \frac{BM \cdot \tan^2\theta}{2} \right)$$

The wall-sided formula is valid only for hull forms that are vertical at the waterline — a fair approximation at small angles but increasingly wrong above about 30°. Real class software uses numerical integration of the actual hull geometry at each angle of heel.

### Large-angle stability and damage stability

IMO's MSC.267(85) intact criteria (implemented in CargoForge-C) cover only the initial stability range to 40°. Class societies increasingly require:

- **Weather criterion** (MSC.267(85) §2.3) — energy balance between wind heeling and GZ righting moment
- **Damage stability** — flooding scenarios with individual compartments breached (SOLAS II-1, probabilistic method)
- **Dynamic stability assessment** — second-generation intact stability criteria (SDC 7) not yet in force but required for novel vessel types

CargoForge-C implements none of these. The `imo_compliant` flag in `AnalysisResult` covers only the six criteria from §2.2 of Part A.

### Hydrostatic accuracy at intermediate drafts

CargoForge-C's `hydro_interpolate` does linear interpolation between adjacent CSV rows. Real hydrostatic tables published in stability booklets use higher-order interpolation (cubic splines or Lagrange) because displacement versus draft is not linear — a ship's hull widens as it emerges. Linear interpolation introduces small but systematic errors, particularly where the waterplane area changes rapidly (near the bilge or near the main deck edge).

!!! note
    The ROADMAP.md tolerance target for DNV-SE-0475 approval is "< 1% on displacement, < 0.01 m on GM." Linear interpolation is likely adequate for table intervals of 0.5 m draft, but this depends on hull form. A box-shaped barge: fine. A full-form tanker with a pronounced bilge radius: less so.

### Longitudinal strength — station count and load distribution

CargoForge-C uses 20 stations (`LS_NUM_STATIONS = 20` in `longitudinal_strength.c`). The lightship weight distribution uses a three-zone trapezoidal approximation (stern 20%, midship, bow 20%). Real class software ingests actual hull girder section modulus curves from the construction drawings and applies the actual longitudinal weight breakdown from the ship's lightship survey. DNV's rules for container ships require bending moment curves at every frame — hundreds of stations.

The simplified distributions mean CargoForge-C's shear force and bending moment outputs are useful for gross checks against permissible limits but would not satisfy class documentation requirements.

### Regulatory scope

CargoForge-C implements the general cargo ship criteria. The ROADMAP.md identifies the next regulatory modules not yet present:

- **International Grain Code** — stability requirements for bulk carriers carrying grain (the grain shifts if the ship heels)
- **MARPOL Annex I** — oil tanker stability in damaged condition
- **CSS Code** — cargo securing manual calculations
- **Second-generation intact stability criteria** (SDC 7) — energy-based dynamic stability

Each of these is a distinct calculation layer that class software ships as a separate licensed module.

---

## CargoForge-C's explicit simplifications

| Area | Simplification | Where in code |
|---|---|---|
| Hull form | Box-hull fallback ($C_b = 0.75$, $C_{wp} = 0.85$) | `analysis.c` — `perform_analysis` |
| $KB$ | $KB = 0.53 \times T$ (constant factor) | `analysis.c` — `KB_FACTOR` |
| GZ curve | Wall-sided formula only | `analysis.c` — `gz_at_angle` |
| Stability range | 0–40° only; no large-angle beyond wall-sided | `analysis.c` — `integrate_gz` |
| Longitudinal stations | 20 stations, trapezoidal lightship distribution | `longitudinal_strength.c` |
| Hydrostatic interpolation | Linear between CSV rows | `hydrostatics.c` — `lerp` / `interpolate_entries` |
| Trim correction | Moment-about-midship formula; no trimmed hydrostatics | `analysis.c` |
| Damage stability | Not implemented | — |
| Weather criterion | Not implemented | — |

None of these are bugs. They are deliberate scope choices that make the code teachable and the physics visible. The wall-sided GZ formula is 30 lines of readable C. NAPA's hull-geometry integrator is years of proprietary numerical analysis. The tradeoff is legibility versus certification.

!!! warning
    Do not use CargoForge-C as a loading computer for a real vessel. It has no class-society approval and several of its models (box-hull KB, wall-sided GZ) are systematically wrong outside their valid ranges.

---

## The approval path

The ROADMAP.md lays out the trajectory toward a DNV type approval under DNV-SE-0475:

1. **Validation against benchmark vessels** — three vessel types (container, bulk, general cargo), tolerance < 1% on displacement and < 0.01 m on GM.
2. **Technical manual** — every equation, every constant, cited to the IMO source.
3. **Test coverage** — the current suite (`make test`) covers 8 binaries and 215+ assertions; DNV requires demonstrably > 95% coverage.
4. **Open-source as advantage** — unlike closed competitors, the source can be read by DNV reviewers directly. "When DNV reviewers can `git blame` the GZ curve implementation, trust goes up."

The ROADMAP.md estimates ~$30,000–$60,000 for the DNV process and 3–6 months after submission. This is not a CargoForge-C course exercise — it is the commercial milestone that converts the engine from "interesting" to "licensable."

---

## Where a learner goes next

If the physics and the C code in this course have sparked genuine interest in naval architecture and maritime software, the natural progression is:

**On the physics side:**
- *Barras, Ship Stability for Masters and Mates* — the standard practitioner reference for intact and damage stability
- IMO Resolution MSC.267(85) — read the actual text; the six criteria in CargoForge-C cite it directly
- DNV-SE-0475 — the benchmark document against which open-source stability software would be validated

**On the software side:**
- Replace the wall-sided GZ formula with a numerical hull-geometry integrator. This requires parsing a hull offset table and doing numerical integration over waterplane sections.
- Implement trimmed hydrostatics — separate CSV tables at multiple trim values, with bilinear interpolation on (draft, trim).
- Explore NAPA's published API documentation (their research division has published papers on the numerical methods)
- GHS (General Hydrostatics by Creative Systems) uses a similar table-interpolation architecture to CargoForge-C but with much richer geometry handling — worth studying as the "minimum viable class-approved" design.

**On certification:**
- DNV-SE-0475 Appendix A lists the full set of test cases required for type approval. Implementing the `validation/validate_benchmark` harness (already in the Makefile as `make validate`) is the first concrete step toward that submission.

---

## Recap

- CargoForge-C covers the core IMO intact stability criteria (MSC.267(85) §2.2) but not damage stability, the weather criterion, or second-generation dynamic criteria.
- Its GZ curve uses the wall-sided formula, which diverges from real hull geometry above ~30° heel.
- The box-hull fallback ($C_b = 0.75$, $KB = 0.53T$) is a teaching approximation; table-interpolation mode matches stability booklet values within linear-interpolation accuracy.
- Longitudinal strength uses 20 stations and a trapezoidal lightship distribution — adequate for gross checks, not class documentation.
- Class-society approval (e.g. DNV-SE-0475) requires validated benchmark runs, a full technical manual, and > 95% test coverage — the ROADMAP.md describes this as the critical gate for operational use.
- The open-source C architecture is a competitive advantage for approval: reviewers can audit the source directly, unlike closed competitors.

*Next: [Lab 12 — A Realistic Voyage](lab-12-capstone.md).*
