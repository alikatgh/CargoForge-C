# Subcommands

## optimize

Runs the 3D bin-packing algorithm and performs stability analysis.

```bash
./cargoforge optimize <ship.cfg> <cargo.txt> [options]
```

**Options:**

| Option | Description |
|--------|-------------|
| `--format=FORMAT` | Output format: `human`, `json`, `csv`, `table`, `markdown` |
| `--output=FILE` | Write output to file instead of stdout |
| `--no-viz` | Disable ASCII visualization |
| `--only-placed` | Show only successfully placed cargo |
| `--only-failed` | Show only cargo that couldn't be placed |
| `--type=TYPE` | Filter output by cargo type |
| `-v, --verbose` | Verbose output |
| `-q, --quiet` | Suppress status messages |

---

## validate

Checks input files for errors without running optimization.

```bash
./cargoforge validate <ship.cfg> <cargo.txt>
```

Returns exit code 0 if valid, non-zero if errors found.

```bash
./cargoforge validate ship.cfg cargo.txt && ./cargoforge optimize ship.cfg cargo.txt
```

---

## info

Displays ship specifications and cargo statistics.

```bash
./cargoforge info <ship.cfg> [cargo.txt]
```

The cargo file is optional. If provided, shows cargo summary statistics.

---

## analyze

Analyzes saved JSON optimization results.

```bash
./cargoforge analyze <results.json>
```

Accepts `-` for stdin:

```bash
./cargoforge optimize ship.cfg cargo.txt --format=json | ./cargoforge analyze -
```

---

## interactive

Launches a step-by-step configuration wizard for creating ship and cargo files.

```bash
./cargoforge interactive
```

---

## version

```bash
./cargoforge version
```

## help

```bash
./cargoforge help
./cargoforge help optimize
```
