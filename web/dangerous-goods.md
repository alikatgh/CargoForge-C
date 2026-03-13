# Dangerous Goods (IMDG)

CargoForge implements the full IMDG Code Table 7.2.4 segregation matrix for dangerous goods compliance.

## Overview

When cargo items carry DG class information, the engine checks all DG pairs against the complete segregation matrix and reports any violations. Without DG info, hazardous cargo falls back to a simple 3m minimum separation rule.

## DG Field Format

Add a 5th field to any cargo line in the manifest:

```
FlammableLiquid   25   6x2.5x2.6   hazardous   DG:3.1:UN1203:A:F-E,S-D
```

Format: `DG:class.division:UNnumber:stowage:EmS`

| Part | Example | Description |
|------|---------|-------------|
| class.division | `3.1` | IMDG class and division |
| UNnumber | `UN1203` | UN number |
| stowage | `A` | `A` = any location, `D` = deck only, `U` = under deck only |
| EmS | `F-E,S-D` | Emergency Schedule reference |

## Segregation Levels

The IMDG matrix defines 6 segregation levels:

| Level | Name | Min Distance | Description |
|-------|------|-------------|-------------|
| 0 | None | 0 m | No segregation required |
| 1 | Away from | 3 m | Minimum 3m horizontal separation |
| 2 | Separated from | 6 m | Minimum 6m horizontal separation |
| 3 | Separated by complete compartment | 12 m | At least one intervening compartment |
| 4 | Separated longitudinally | 24 m | Full longitudinal hold separation |
| 5 | Incompatible | N/A | Must not be loaded on the same vessel |

## IMDG Classes

The 17x17 matrix covers all 9 IMDG classes with their subdivisions:

| Index | Class | Description |
|-------|-------|-------------|
| 0 | 1.1-1.6 | Explosives (mass explosion to insensitive) |
| 1 | 1.7 | Explosives (not mass explosion hazard) |
| 2 | 1.8 | Explosives (insensitive detonating) |
| 3 | 2.1 | Flammable gases |
| 4 | 2.2 | Non-flammable, non-toxic gases |
| 5 | 2.3 | Toxic gases |
| 6 | 3 | Flammable liquids |
| 7 | 4.1 | Flammable solids |
| 8 | 4.2 | Spontaneously combustible |
| 9 | 4.3 | Dangerous when wet |
| 10 | 5.1 | Oxidizing substances |
| 11 | 5.2 | Organic peroxides |
| 12 | 6.1 | Toxic substances |
| 13 | 6.2 | Infectious substances |
| 14 | 7 | Radioactive material |
| 15 | 8 | Corrosive substances |
| 16 | 9 | Miscellaneous dangerous substances |

## Key Rules

- **Class 1 (Explosives)** — Incompatible with all other classes
- **Class 9 (Miscellaneous)** — No segregation required with most classes
- **The matrix is symmetric** — A vs B gives the same result as B vs A
- **Distance is edge-to-edge** — Measured between nearest surfaces of the cargo items

## Example

```
# Three DG items that will be checked pairwise
FlammLiquid   25   6x2.5x2.6   hazardous   DG:3.1:UN1203:A:F-E,S-D
Oxidizer      15   6x2.5x2.6   hazardous   DG:5.1:UN1942:A:F-A,S-Q
Corrosive     20   6x2.5x2.6   hazardous   DG:8.0:UN1789:A:F-A,S-B
```

The optimizer will check:
- Class 3.1 vs 5.1: **Separated from** (min 6m)
- Class 3.1 vs 8.0: **Away from** (min 3m)
- Class 5.1 vs 8.0: **Separated from** (min 6m)

If any pair is too close, the output lists each violation with the required vs actual distance.

## Fallback Behavior

When no DG info is provided (cargo typed as `hazardous` but no 5th field), the legacy 3m minimum separation rule applies to all hazardous cargo pairs.
