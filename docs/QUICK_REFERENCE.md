# CargoForge-C Quick Reference

**One-Page Cheat Sheet**

---

## Subcommands

| Command | Purpose | Example |
|---------|---------|---------|
| `optimize` | Run optimization | `cargoforge optimize ship.cfg cargo.txt` |
| `validate` | Validate inputs | `cargoforge validate ship.cfg cargo.txt` |
| `info` | Show ship info | `cargoforge info ship.cfg` |
| `analyze` | Analyze JSON results | `cargoforge analyze results.json` |
| `interactive` | Guided wizard | `cargoforge interactive` |
| `version` | Show version | `cargoforge version` |
| `help` | Show help | `cargoforge help` |

---

## Common Options

| Option | Description |
|--------|-------------|
| `--format=FORMAT` | Output format: human\|json\|csv\|table\|markdown |
| `--output=FILE` | Save output to file |
| `-v, --verbose` | Verbose logging |
| `-q, --quiet` | Suppress output |
| `--no-color` | Disable colors |
| `--no-viz` | Disable ASCII visualization |
| `--only-placed` | Show only placed cargo |
| `--only-failed` | Show only failed cargo |
| `--type=TYPE` | Filter by cargo type |
| `-h, --help` | Show help |

---

## Output Formats

```bash
# Human-readable (default)
cargoforge optimize ship.cfg cargo.txt

# JSON for APIs
cargoforge optimize ship.cfg cargo.txt --format=json

# CSV for Excel
cargoforge optimize ship.cfg cargo.txt --format=csv > data.csv

# Professional table
cargoforge optimize ship.cfg cargo.txt --format=table

# Markdown report
cargoforge optimize ship.cfg cargo.txt --format=markdown > REPORT.md
```

---

## Stdin/Stdout

```bash
# Read from pipe
cat cargo.txt | cargoforge optimize ship.cfg -

# Chain commands
cargoforge optimize ship.cfg cargo.txt --format=json | cargoforge analyze -

# Save and analyze
cargoforge optimize ship.cfg cargo.txt --format=json | tee results.json | cargoforge analyze -

# Filter input
grep "Container" cargo.txt | cargoforge optimize ship.cfg -
```

---

## Configuration File

**Location**: `~/.cargoforgerc` or `./.cargoforgerc`

**Format**:
```ini
# Comment
format=table
color=yes
verbose=no
quiet=no
show_viz=yes
algorithm=3d
```

**Quick setup**:
```bash
# Global config
echo "format=table" > ~/.cargoforgerc

# Project config
echo "format=json" > .cargoforgerc
```

---

## Common Workflows

### Validate → Optimize → Analyze

```bash
cargoforge validate ship.cfg cargo.txt && \
  cargoforge optimize ship.cfg cargo.txt --format=json --output=results.json && \
  cargoforge analyze results.json
```

### Batch Processing

```bash
for cargo in *.txt; do
  cargoforge optimize ship.cfg "$cargo" --quiet --format=json \
    --output="${cargo%.txt}.json"
done
```

### Full Analysis (Hydro + Tanks + Strength + DG)

```bash
cargoforge optimize ship_full.cfg cargo_dg.txt --format=json --output=results.json && \
  cargoforge analyze results.json
```

### API Integration

```bash
# Silent JSON output
cargoforge optimize ship.cfg cargo.txt --quiet --format=json
```

---

## Input File Formats

### Ship Config (.cfg)

```
# Minimal (box-hull fallback)
length_m=150
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0

# Optional: real hydrostatics, tanks, strength limits
hydrostatic_table=examples/sample_hydro.csv
tank_config=examples/sample_tanks.csv
permissible_sf_tonnes=5000
permissible_bm_hog_t_m=120000
permissible_bm_sag_t_m=100000
```

### Cargo Manifest (.txt)

```
# ID  Weight(t)  Dimensions  Type  [DG info]
HeavyMachinery  550  20x5x3  standard
ContainerA  250  12.2x2.4x2.6  reefer
FlammLiquid  25  6x2.5x2.6  hazardous  DG:3.1:UN1203:A:F-E,S-D
```

**Cargo Types**: `standard`, `hazardous`, `reefer`, `fragile`, `heavy`, `bulk`

**DG field**: `DG:class.division:UNnumber:stowage:EmS` (optional, enables full IMDG matrix)

### Hydrostatic Table (.csv)

```
# draft_m,displacement_t,KM_m,KB_m,BM_m,TPC_t_cm,MTC_t_m[,waterplane_m2,LCB_m]
2.0,2306,13.20,1.06,12.14,9.60,185.0,3375,1.5
```

### Tank Config (.csv)

```
# id,length_m,breadth_m,height_m,pos_x_m,pos_y_m,pos_z_m,fill_fraction,density_t_m3
BallastFP,8.0,12.0,6.0,140.0,0.0,0.0,0.50,1.025
```

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Invalid arguments |
| 2 | File error |
| 3 | Parse error |
| 4 | Optimization error |
| 5 | Validation error |

---

## One-Liners

```bash
# Quick validation
cargoforge validate ship.cfg cargo.txt --quiet && echo "✓" || echo "✗"

# Extract GM value (corrected for free surface if tanks loaded)
cargoforge optimize ship.cfg cargo.txt --format=json --quiet | \
  jq -r '.analysis.gm_corrected // .analysis.metacentric_height'

# Check longitudinal strength compliance
cargoforge optimize ship.cfg cargo.txt --format=json --quiet | \
  jq -r '.analysis.longitudinal_strength.compliant'

# Count placed items
cargoforge optimize ship.cfg cargo.txt --format=json --quiet | \
  jq -r '.analysis.placed_count'

# Failed items only
cargoforge optimize ship.cfg cargo.txt --only-failed --format=table

# Silent automation
cargoforge optimize ship.cfg cargo.txt --quiet --format=json > output.json 2>/dev/null

# Full analysis with hydro tables and DG cargo
cargoforge optimize ship_full.cfg cargo_dg.txt --format=json --quiet | \
  jq '{gm: .analysis.gm_corrected, strength: .analysis.longitudinal_strength}'
```

---

## Debug Commands

```bash
# Verbose mode
cargoforge optimize ship.cfg cargo.txt --verbose

# Full debug log
cargoforge optimize ship.cfg cargo.txt --verbose --no-color 2>&1 | tee debug.log

# Validate with details
cargoforge validate ship.cfg cargo.txt --verbose
```

---

## Integration Snippets

### Python

```python
import subprocess, json

result = subprocess.run(
    ['cargoforge', 'optimize', 'ship.cfg', 'cargo.txt', '--format=json', '--quiet'],
    capture_output=True, text=True
)
data = json.loads(result.stdout)
print(f"Placed: {data['analysis']['placed_count']}")
```

### Bash Script

```bash
#!/bin/bash
if cargoforge validate ship.cfg cargo.txt --quiet; then
  cargoforge optimize ship.cfg cargo.txt --format=json --output=results.json
else
  echo "Validation failed"
  exit 1
fi
```

### cURL (via wrapper API)

```bash
curl -X POST http://localhost:5000/api/optimize \
  -H "Content-Type: application/json" \
  -d @request.json
```

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Command not found | Use `./cargoforge` or add to PATH |
| Parse error | Run `cargoforge validate` to check files |
| No color | Terminal doesn't support ANSI, use `--no-color` |
| Stdin not working | Make sure to use `-` as filename |
| Permission denied | Check file permissions with `ls -l` |

---

## Quick Help

```bash
# General help
cargoforge --help

# Subcommand help
cargoforge optimize --help

# Version
cargoforge --version

# Examples
cargoforge help
```

---

## Useful Aliases

Add to `~/.bashrc` or `~/.zshrc`:

```bash
# Short aliases
alias cfo='cargoforge optimize'
alias cfv='cargoforge validate'
alias cfi='cargoforge info'
alias cfa='cargoforge analyze'

# Common workflows
alias cf-json='cargoforge optimize --format=json --quiet'
alias cf-table='cargoforge optimize --format=table'
alias cf-validate='cargoforge validate --quiet && echo "✓ Valid" || echo "✗ Invalid"'
```

---

**For full documentation, see [CLI_GUIDE.md](CLI_GUIDE.md) and [EXAMPLES.md](EXAMPLES.md)**
