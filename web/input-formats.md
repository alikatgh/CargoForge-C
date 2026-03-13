# Input File Formats

## Ship Configuration (.cfg)

Key-value pairs defining the vessel. Lines starting with `#` are comments.

### Minimal configuration (box-hull fallback)

```
# Ship Specifications
length_m=150
width_m=25
max_weight_tonnes=50000

# Lightship data for stability calculations
lightship_weight_tonnes=2000
lightship_kg_m=8.0
```

### Full configuration (hydrostatic tables, tanks, strength limits)

```
length_m=150
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0

# Hydrostatic table (enables real stability booklet data)
hydrostatic_table=examples/sample_hydro.csv

# Tank configuration (enables free surface correction)
tank_config=examples/sample_tanks.csv

# Permissible longitudinal strength limits (from class society)
permissible_sf_tonnes=5000
permissible_bm_hog_t_m=120000
permissible_bm_sag_t_m=100000
```

### Ship Config Fields

| Field | Unit | Description | Required |
|-------|------|-------------|----------|
| `length_m` | meters | Length overall (LOA) | Yes |
| `width_m` | meters | Beam (breadth) | Yes |
| `max_weight_tonnes` | tonnes | Maximum displacement | Yes |
| `lightship_weight_tonnes` | tonnes | Empty ship weight | Yes |
| `lightship_kg_m` | meters | Lightship vertical CG above keel | Yes |
| `hydrostatic_table` | path | Path to hydrostatic CSV file | No |
| `tank_config` | path | Path to tank configuration CSV file | No |
| `permissible_sf_tonnes` | tonnes | Max allowable shear force | No |
| `permissible_bm_hog_t_m` | t-m | Max allowable hogging bending moment | No |
| `permissible_bm_sag_t_m` | t-m | Max allowable sagging bending moment | No |

All optional fields enable their respective features when present. When absent, the analysis falls back to simpler models (box-hull hydrostatics, no free surface correction, no strength check).

---

## Cargo Manifest (.txt)

Space-separated columns. Lines starting with `#` are comments.

### Basic format

```
# ID              Weight(t)  Dimensions(LxWxH)  Type
HeavyMachinery    550        20x5x3             standard
SteelBeams        400        18x2x2             bulk
ContainerA        250        12.2x2.4x2.6       reefer
GlassPanel        30         4x3x1              fragile
```

### With dangerous goods (DG) information

The optional 5th field enables full IMDG Code segregation:

```
# ID              Weight(t)  Dimensions(LxWxH)  Type       DG info
FlammableLiquid   25         6x2.5x2.6          hazardous  DG:3.1:UN1203:A:F-E,S-D
OxidizerDrum      15         6x2.5x2.6          hazardous  DG:5.1:UN1942:A:F-A,S-Q
CorrosiveAcid     20         6x2.5x2.6          hazardous  DG:8.0:UN1789:A:F-A,S-B
```

### Column Reference

| Column | Format | Description |
|--------|--------|-------------|
| ID | string | Unique identifier (max 31 chars) |
| Weight | float | Weight in tonnes |
| Dimensions | `LxWxH` | Length x Width x Height in meters |
| Type | string | Cargo classification (see below) |
| DG info | `DG:class.div:UN:stow:EmS` | Optional IMDG dangerous goods info |

### DG Field Format

`DG:class.division:UNnumber:stowage:EmS`

| Part | Example | Description |
|------|---------|-------------|
| class.division | `3.1` | IMDG class and division |
| UNnumber | `UN1203` | UN number |
| stowage | `A` | `A` = any, `D` = deck only, `U` = under deck only |
| EmS | `F-E,S-D` | Emergency Schedule reference |

When DG info is present, the full IMDG Code Table 7.2.4 segregation matrix is used instead of the basic 3m separation rule.

### Cargo Types

| Type | Constraints Applied |
|------|-------------------|
| `standard` | Point load, deck weight ratio |
| `hazardous` | IMDG segregation (full matrix if DG info present, 3m fallback otherwise) |
| `reefer` | Prefers deck placement (reefer plug access) |
| `fragile` | Lower stacking pressure limit (5 t/m2 vs 20 t/m2) |
| `heavy` | Point load, deck weight ratio |
| `bulk` | Standard constraints |

---

## Hydrostatic Table (.csv)

CSV file with real hydrostatic data from the vessel's stability booklet. Lines starting with `#` are comments. Entries must be in ascending draft order. At least 2 entries required.

```csv
# draft_m,displacement_t,KM_m,KB_m,BM_m,TPC_t_cm,MTC_t_m,waterplane_m2,LCB_m
2.0,2306,13.20,1.06,12.14,9.60,185.0,3375,1.5
3.0,3544,9.92,1.58,8.34,10.20,195.0,3570,1.2
4.0,4838,8.23,2.10,6.13,10.70,205.0,3720,0.9
5.0,6181,7.25,2.63,4.62,11.10,215.0,3840,0.6
6.0,7569,6.68,3.14,3.54,11.40,225.0,3930,0.3
```

| Column | Unit | Description |
|--------|------|-------------|
| draft_m | m | Even-keel draft |
| displacement_t | tonnes | Displacement at this draft |
| KM_m | m | Height of transverse metacentre above keel |
| KB_m | m | Centre of buoyancy above keel |
| BM_m | m | Transverse metacentric radius |
| TPC_t_cm | t/cm | Tonnes per cm immersion |
| MTC_t_m | t-m | Moment to change trim 1 cm |
| waterplane_m2 | m2 | Waterplane area (optional, can be 0) |
| LCB_m | m | Longitudinal centre of buoyancy from midship (optional) |

The last two columns are optional. A 7-column format (without waterplane and LCB) is also accepted.

---

## Tank Configuration (.csv)

CSV file defining the vessel's liquid tanks for free surface correction. Lines starting with `#` are comments.

```csv
# id,length_m,breadth_m,height_m,pos_x_m,pos_y_m,pos_z_m,fill_fraction,density_t_m3
BallastFP,8.0,12.0,6.0,140.0,0.0,0.0,0.50,1.025
BallastDB_P,20.0,4.0,6.0,75.0,-10.0,0.0,0.75,1.025
FuelOil_1,10.0,6.0,5.0,30.0,-8.0,0.0,0.60,0.950
FreshWater,5.0,5.0,4.0,20.0,0.0,0.0,0.85,1.000
```

| Column | Unit | Description |
|--------|------|-------------|
| id | string | Tank identifier |
| length_m | m | Tank length along ship |
| breadth_m | m | Tank breadth athwartships |
| height_m | m | Tank height |
| pos_x_m | m | Longitudinal position from AP |
| pos_y_m | m | Transverse position from centreline |
| pos_z_m | m | Vertical position of tank bottom above keel |
| fill_fraction | 0-1 | Fill level (0.0 = empty, 1.0 = full) |
| density_t_m3 | t/m3 | Liquid density (seawater=1.025, fuel oil=0.95, fresh water=1.0) |

Only partially filled tanks (0 < fill < 1) produce free surface effects. Full and empty tanks have no free surface moment.
