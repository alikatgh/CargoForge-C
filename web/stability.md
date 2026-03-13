# Stability Analysis

## Hydrostatics Source

The output indicates which hydrostatic model is active:

| Indicator | Meaning |
|-----------|---------|
| `[Hydrostatic table]` | Real stability booklet data interpolated from CSV |
| `[Box-hull model]` | Simplified coefficients (Cb=0.75) — no hydrostatic table loaded |

## Hydrostatic Parameters

| Parameter | Meaning |
|-----------|---------|
| **Draft** | How deep the ship sits in the water (meters) |
| **KG** | Vertical center of gravity above keel |
| **KB** | Vertical center of buoyancy above keel |
| **BM** | Metacentric radius (distance from B to M) |
| **GM** | Metacentric height = KB + BM - KG. Positive = stable |
| **GM (corrected)** | GM after free surface correction. Shown when tanks are loaded |
| **Free surface correction** | Virtual KG rise from partially filled tanks (meters) |

## GM Interpretation

| GM Range | Status | Meaning |
|----------|--------|---------|
| < 0.15 m | CRITICAL | Vessel may capsize. IMO non-compliant |
| 0.15 - 0.30 m | Tender | Stable but marginal |
| 0.30 - 0.50 m | Acceptable | Within safe range |
| 0.50 - 2.50 m | Optimal | Comfortable motion, safe |
| 2.50 - 3.00 m | Acceptable | Getting stiff |
| > 3.00 m | Too Stiff | Uncomfortable rolling, structural stress risk |

When tank data is loaded, the **corrected GM** (after free surface correction) is used for all stability assessments and IMO criteria.

## Trim

Positive trim = stern sits deeper than bow. Negative = bow deeper. Ideally close to zero or slightly by stern.

## Heel

Degrees of permanent list to one side. Should be < 1 degree for a well-loaded ship. Caused by asymmetric cargo placement.

---

## IMO Intact Stability Criteria

Based on MSC.267(85) Part A, Chapter 2.2. All six criteria must pass:

| Criterion | Minimum | What It Means |
|-----------|---------|---------------|
| GM >= 0.15 m | 0.15 m | Basic positive stability |
| GZ at 30 deg >= 0.20 m | 0.20 m | Righting arm at large heel |
| Max GZ angle >= 25 deg | 25 deg | Peak restoring force not too early |
| Area 0-30 deg | 0.055 m-rad | Energy to resist heeling to 30 deg |
| Area 0-40 deg | 0.090 m-rad | Energy to resist heeling to 40 deg |
| Area 30-40 deg | 0.030 m-rad | Reserve stability at large angles |

GM is corrected for free surface effects when tank data is loaded. GZ curve computed using the wall-sided formula with trapezoidal integration.

---

## Longitudinal Strength

When permissible strength limits are configured in the ship config, CargoForge calculates still water shear force and bending moment across 21 hull stations and checks against class society limits.

| Parameter | Meaning |
|-----------|---------|
| **Max SF** | Peak shear force along the hull (tonnes) |
| **Max BM (hog)** | Peak hogging bending moment (t-m) |
| **Max BM (sag)** | Peak sagging bending moment (t-m) |
| **SF ratio** | max_sf / permissible_sf (must be <= 1.0) |
| **BM ratio** | max_bm / permissible_bm (must be <= 1.0) |
| **Status** | WITHIN LIMITS or EXCEEDS LIMITS |

### How It Works

1. **Lightship weight** is distributed as a trapezoidal curve (heavier amidships, lighter at bow/stern)
2. **Cargo weight** is distributed at actual placement positions
3. **Buoyancy** is distributed uniformly with 5% bow/stern taper, normalized to match displacement
4. **Net load** = weight - buoyancy at each station
5. **Shear force** = integral of net load from stern
6. **Bending moment** = integral of shear force from stern

### Required Config

```
permissible_sf_tonnes=5000
permissible_bm_hog_t_m=120000
permissible_bm_sag_t_m=100000
```

These values come from the vessel's class society (Lloyd's, DNV, Bureau Veritas, etc.).

---

## Free Surface Correction

Partially filled liquid tanks reduce effective stability by raising the virtual center of gravity.

### How It Works

1. Each partially filled tank produces a **free surface moment (FSM)**:

   `FSM = density * length * breadth^3 / 12`

2. Total FSM is summed across all tanks where `0 < fill < 1`
3. **Virtual KG rise** = total_FSM / displacement
4. **Corrected GM** = GM - virtual_KG_rise

Full tanks (fill = 1.0) and empty tanks (fill = 0.0) produce zero FSM.

### Required Config

```
tank_config=examples/sample_tanks.csv
```

See [Tank Configuration](input-formats.md?id=tank-configuration-csv) for the CSV format.
