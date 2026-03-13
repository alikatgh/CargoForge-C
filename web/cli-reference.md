# CLI Reference

## Common Options

| Option | Description |
|--------|-------------|
| `--format=FORMAT` | Output format: human, json, csv, table, markdown |
| `--output=FILE` | Save output to file |
| `-v, --verbose` | Verbose logging |
| `-q, --quiet` | Suppress output |
| `--no-color` | Disable colors |
| `--no-viz` | Disable ASCII visualization |
| `--only-placed` | Show only placed cargo |
| `--only-failed` | Show only failed cargo |
| `--type=TYPE` | Filter by cargo type |
| `-h, --help` | Show help |

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Invalid arguments |
| 2 | File error |
| 3 | Parse error |
| 4 | Optimization error |
| 5 | Validation error |

## Stdin/Stdout

```bash
# Read from pipe
cat cargo.txt | cargoforge optimize ship.cfg -

# Chain commands
cargoforge optimize ship.cfg cargo.txt --format=json | cargoforge analyze -

# Tee for multiple outputs
cargoforge optimize ship.cfg cargo.txt --format=json | tee results.json | cargoforge analyze -
```

## One-Liners

```bash
# Quick validation
cargoforge validate ship.cfg cargo.txt --quiet && echo "OK" || echo "FAILED"

# Extract corrected GM
cargoforge optimize ship.cfg cargo.txt --format=json --quiet | \
  jq -r '.analysis.gm_corrected // .analysis.metacentric_height'

# Check longitudinal strength
cargoforge optimize ship.cfg cargo.txt --format=json --quiet | \
  jq -r '.analysis.longitudinal_strength.compliant'

# Count placed items
cargoforge optimize ship.cfg cargo.txt --format=json --quiet | \
  jq -r '.analysis.placed_count'

# Full analysis with hydro tables and DG
cargoforge optimize ship_full.cfg cargo_dg.txt --format=json --quiet | \
  jq '{gm: .analysis.gm_corrected, strength: .analysis.longitudinal_strength}'
```

## Useful Aliases

```bash
alias cfo='cargoforge optimize'
alias cfv='cargoforge validate'
alias cfi='cargoforge info'
alias cfa='cargoforge analyze'
alias cf-json='cargoforge optimize --format=json --quiet'
alias cf-table='cargoforge optimize --format=table'
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Command not found | Use `./cargoforge` or add to PATH |
| Parse error | Run `cargoforge validate` to check files |
| No color | Terminal doesn't support ANSI, use `--no-color` |
| Stdin not working | Use `-` as filename |
