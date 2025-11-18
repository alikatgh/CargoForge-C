# CargoForge-C CLI Guide

**Complete Command-Line Interface Documentation**

Version: 2.1.0
Last Updated: November 2025

---

## Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Subcommands](#subcommands)
5. [Global Options](#global-options)
6. [Output Formats](#output-formats)
7. [Configuration Files](#configuration-files)
8. [Stdin/Stdout Operations](#stdinstdout-operations)
9. [Advanced Usage](#advanced-usage)
10. [Troubleshooting](#troubleshooting)

---

## Introduction

CargoForge-C is a professional maritime cargo optimization tool with a modern, Unix-friendly command-line interface. The CLI supports:

- **7 subcommands** for different operations
- **5 output formats** (human, JSON, CSV, table, markdown)
- **Stdin/stdout piping** for Unix integration
- **Configuration files** for persistent defaults
- **Colored output** with auto-detection
- **Interactive mode** for guided workflows

### Design Philosophy

The CLI follows Unix philosophy:
- **Do one thing well** - Each subcommand has a focused purpose
- **Text streams** - Work with pipes and redirects
- **Silent success** - Use quiet mode for scripting
- **Composable** - Chain commands together
- **Sensible defaults** - Works out of the box

---

## Installation

### Building from Source

```bash
# Clone repository
git clone https://github.com/alikatgh/CargoForge-C.git
cd CargoForge-C

# Build
make clean && make

# Verify installation
./cargoforge --version
```

### System Requirements

- **OS**: Linux, macOS, or WSL on Windows
- **Compiler**: GCC 9+ or Clang 10+
- **C Standard**: C99
- **Dependencies**: None (only standard C library + libm)

### Optional: Install System-Wide

```bash
# Copy to system binary directory
sudo cp cargoforge /usr/local/bin/

# Now use from anywhere
cargoforge --version
```

---

## Quick Start

### Basic Optimization

```bash
# Run optimization with default settings
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

### Validate Input Files

```bash
# Check files before optimization
./cargoforge validate examples/sample_ship.cfg examples/sample_cargo.txt
```

### Get Ship Information

```bash
# Display ship specifications
./cargoforge info examples/sample_ship.cfg
```

### Save Results as JSON

```bash
# Export results for later analysis
./cargoforge optimize ship.cfg cargo.txt --format=json --output=results.json
```

### Analyze Saved Results

```bash
# Detailed analysis with recommendations
./cargoforge analyze results.json
```

---

## Subcommands

### optimize

**Purpose**: Run cargo optimization algorithm

**Syntax**:
```bash
cargoforge optimize <ship_config> <cargo_manifest> [options]
```

**Arguments**:
- `ship_config` - Ship configuration file (.cfg format)
- `cargo_manifest` - Cargo manifest file (.txt format) or `-` for stdin

**Options**:
- `--format=FORMAT` - Output format (human|json|csv|table|markdown)
- `--output=FILE` - Write output to file
- `--algorithm=ALGO` - Algorithm choice (3d|2d|auto), default: 3d
- `--no-viz` - Disable ASCII visualization
- `--only-placed` - Show only successfully placed cargo
- `--only-failed` - Show only failed cargo items
- `--type=TYPE` - Filter by cargo type
- `-v, --verbose` - Enable detailed logging
- `-q, --quiet` - Suppress non-essential output
- `--no-color` - Disable colored output

**Examples**:
```bash
# Basic optimization
cargoforge optimize ship.cfg cargo.txt

# JSON output for API
cargoforge optimize ship.cfg cargo.txt --format=json

# Save to file
cargoforge optimize ship.cfg cargo.txt --output=results.csv --format=csv

# Verbose mode for debugging
cargoforge optimize ship.cfg cargo.txt --verbose

# Filter failed items only
cargoforge optimize ship.cfg cargo.txt --only-failed

# Read cargo from stdin
cat cargo.txt | cargoforge optimize ship.cfg -
```

**Exit Codes**:
- `0` - Success
- `1` - Invalid arguments
- `2` - File error
- `3` - Parse error
- `4` - Optimization error

---

### validate

**Purpose**: Validate input files without running optimization

**Syntax**:
```bash
cargoforge validate <ship_config> <cargo_manifest> [options]
```

**Options**:
- `--format=FORMAT` - Output format for validation report
- `-v, --verbose` - Show detailed validation information
- `--no-color` - Disable colored output

**Examples**:
```bash
# Basic validation
cargoforge validate ship.cfg cargo.txt

# Verbose validation with details
cargoforge validate ship.cfg cargo.txt --verbose

# Chain with optimization
cargoforge validate ship.cfg cargo.txt && \
  cargoforge optimize ship.cfg cargo.txt
```

**What is Validated**:
- Ship configuration format and values
- Cargo manifest format and values
- Total weight vs ship capacity
- Dimension formats (LxWxH)
- Cargo type validity
- File syntax errors

**Exit Codes**:
- `0` - All validations passed
- `5` - Validation errors found

---

### info

**Purpose**: Display ship and cargo statistics

**Syntax**:
```bash
cargoforge info <ship_config> [cargo_manifest] [options]
```

**Options**:
- `--format=FORMAT` - Output format (human|json|table|markdown)
- `--no-color` - Disable colored output

**Examples**:
```bash
# Ship info only
cargoforge info ship.cfg

# Ship and cargo info
cargoforge info ship.cfg cargo.txt

# JSON output
cargoforge info ship.cfg --format=json
```

**Output Includes**:
- Ship dimensions and capacity
- Deck area calculation
- Lightship weight and KG
- Cargo count and total weight
- Capacity utilization percentage
- Cargo type breakdown (hazardous, reefer, fragile counts)

---

### analyze

**Purpose**: Analyze saved JSON optimization results

**Syntax**:
```bash
cargoforge analyze <results.json> [options]
```

**Arguments**:
- `results.json` - JSON file from previous optimization, or `-` for stdin

**Options**:
- `-q, --quiet` - Suppress header information
- `--no-color` - Disable colored output

**Examples**:
```bash
# Analyze saved results
cargoforge analyze results.json

# Analyze from pipe
cargoforge optimize ship.cfg cargo.txt --format=json | cargoforge analyze -

# Save analysis to file
cargoforge analyze results.json > analysis_report.txt
```

**Output Sections**:
1. **Ship Specifications** - Dimensions, capacity, deck area
2. **Cargo Summary** - Placement statistics and percentages
3. **Weight Analysis** - Total weight, capacity utilization
4. **Stability Analysis** - Center of gravity, metacentric height
5. **Recommendations** - Smart suggestions based on results

**Stability Indicators**:
- ✅ **OPTIMAL** - GM between 0.5m and 2.5m
- ⚠️ **TOO STIFF** - GM above 2.5m (uncomfortable ride)
- ❌ **UNSTABLE** - GM below 0.5m (safety risk)

---

### interactive

**Purpose**: Launch interactive configuration wizard

**Syntax**:
```bash
cargoforge interactive
```

**Features**:
- Step-by-step ship configuration
- Guided cargo entry
- Input validation with immediate feedback
- Option to run optimization after creation

**Example Session**:
```
╔══════════════════════════════════════════════════════════╗
║      CargoForge-C Interactive Configuration Wizard      ║
╚══════════════════════════════════════════════════════════╝

STEP 1: Ship Configuration
---------------------------
Enter ship length (meters): 150
Enter ship width (meters): 25
Enter maximum cargo weight (tonnes): 50000
Enter filename to save ship config: my_ship.cfg

✓ Ship configuration saved

STEP 2: Cargo Configuration
----------------------------
How many cargo items? 3
...
```

---

### version

**Purpose**: Display version information

**Syntax**:
```bash
cargoforge version
cargoforge --version
```

**Output**:
```
CargoForge-C version 2.1.0
Build date: Nov 17 2025
Maritime cargo optimization engine

Copyright (c) 2025 CargoForge Project
Licensed under MIT License
```

---

### help

**Purpose**: Display help information

**Syntax**:
```bash
cargoforge help [subcommand]
cargoforge --help
cargoforge <subcommand> --help
```

**Examples**:
```bash
# General help
cargoforge help

# Subcommand-specific help
cargoforge help optimize
cargoforge optimize --help
```

---

## Global Options

Options that work with all subcommands:

### -h, --help
Show help message for the subcommand

### -v, --verbose
Enable verbose output with detailed logging

### -q, --quiet
Suppress non-essential output (useful for scripting)

### --no-color
Disable ANSI colored output (auto-detected by default)

### --version
Display version information

---

## Output Formats

### Human (default)

Human-readable text with optional ASCII visualization.

**Example**:
```
--- CargoForge Stability Analysis ---

Ship Specs: 150.00 m × 25.00 m
  - HeavyMachinery  | Pos: (0.00, 0.00) | 550.00 t
  - ContainerA      | Pos: (0.00, 0.00) | 250.00 t

[ASCII Visualization]
```

**Best for**: Terminal viewing, debugging

---

### JSON

Machine-readable JSON format for API integration.

**Example**:
```json
{
  "ship": {
    "length": 150.00,
    "width": 25.00,
    "max_weight": 50000000.00
  },
  "cargo": [
    {
      "id": "HeavyMachinery",
      "weight": 550000.00,
      "dimensions": [20.00, 5.00, 3.00],
      "type": "standard",
      "position": {"x": 0.00, "y": 0.00, "z": -8.00},
      "placed": true
    }
  ],
  "analysis": {
    "placed_count": 5,
    "total_count": 5,
    "metacentric_height": 47.06,
    "stability_status": "overstiff"
  }
}
```

**Best for**: API integration, web services, data processing

**Usage**:
```bash
# Save to file
cargoforge optimize ship.cfg cargo.txt --format=json --output=results.json

# Pipe to jq for parsing
cargoforge optimize ship.cfg cargo.txt --format=json | jq '.analysis'
```

---

### CSV

Comma-separated values for spreadsheet import.

**Example**:
```csv
ID,Weight_kg,Length_m,Width_m,Height_m,Type,Placed,Pos_X,Pos_Y,Pos_Z
HeavyMachinery,550000.00,20.00,5.00,3.00,standard,yes,0.00,0.00,-8.00
ContainerA,250000.00,12.20,2.40,2.60,reefer,yes,0.00,0.00,-5.00
```

**Best for**: Excel, Google Sheets, data analysis

**Usage**:
```bash
# Export to CSV
cargoforge optimize ship.cfg cargo.txt --format=csv > results.csv

# Open in Excel
cargoforge optimize ship.cfg cargo.txt --format=csv > data.csv
libreoffice data.csv
```

---

### Table

Professional ASCII table with Unicode box drawing.

**Example**:
```
┌──────────────────┬───────────┬────────────────────┬──────────────┬─────────┬────────────────────────┐
│ Cargo ID         │ Weight(t) │ Dimensions (LxWxH) │ Type         │ Placed  │ Position (X,Y,Z)       │
├──────────────────┼───────────┼────────────────────┼──────────────┼─────────┼────────────────────────┤
│ HeavyMachinery   │    550.00 │ 20.0x5.0x3.0        │ standard     │ YES     │ (0.0, 0.0, -8.0)       │
│ ContainerA       │    250.00 │ 12.2x2.4x2.6        │ reefer       │ YES     │ (0.0, 0.0, -5.0)       │
└──────────────────┴───────────┴────────────────────┴──────────────┴─────────┴────────────────────────┘
```

**Best for**: Terminal output, reports, presentations

**Usage**:
```bash
# Display table
cargoforge optimize ship.cfg cargo.txt --format=table

# Save table to file
cargoforge optimize ship.cfg cargo.txt --format=table > report.txt
```

---

### Markdown

Markdown-formatted tables and reports.

**Example**:
```markdown
# CargoForge-C Optimization Report

## Ship Specifications

| Property | Value |
|----------|-------|
| Length | 150.00 m |
| Width | 25.00 m |

## Cargo Placement Results

| Cargo ID | Weight (t) | Status | Position |
|----------|------------|--------|----------|
| HeavyMachinery | 550.00 | ✅ Placed | (0.0, 0.0, -8.0) |
```

**Best for**: Documentation, GitHub, reports

**Usage**:
```bash
# Generate markdown report
cargoforge optimize ship.cfg cargo.txt --format=markdown > REPORT.md

# View on GitHub
git add REPORT.md && git commit -m "Add optimization report"
```

---

## Configuration Files

### Overview

CargoForge-C supports configuration files to set default options, reducing the need for repeated command-line flags.

### File Locations

1. **Global**: `~/.cargoforgerc` - User-wide defaults
2. **Project**: `./.cargoforgerc` - Project-specific settings

**Priority** (highest to lowest):
1. Command-line flags
2. Project config (`./.cargoforgerc`)
3. Global config (`~/.cargoforgerc`)
4. Built-in defaults

### File Format

Simple `key=value` pairs with comment support:

```ini
# CargoForge-C Configuration
# Lines starting with # are comments

# Output format
format=table

# Enable colors
color=yes

# Verbosity
verbose=no
quiet=no

# Visualization
show_viz=yes

# Algorithm
algorithm=3d
```

### Supported Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `format` | human, json, csv, table, markdown | human | Default output format |
| `color` | yes, no, true, false, 1, 0 | auto | Enable colored output |
| `verbose` | yes, no | no | Verbose logging |
| `quiet` | yes, no | no | Quiet mode |
| `show_viz` | yes, no | yes | ASCII visualization |
| `algorithm` | 3d, 2d, auto | 3d | Optimization algorithm |

### Examples

**API Integration**:
```ini
# .cargoforgerc - API mode
format=json
quiet=yes
color=no
show_viz=no
```

**Debugging**:
```ini
# .cargoforgerc - Debug mode
verbose=yes
color=yes
show_viz=yes
format=table
```

**Production Reports**:
```ini
# .cargoforgerc - Report mode
format=markdown
quiet=no
show_viz=no
```

### Creating Config Files

```bash
# Global config
cat > ~/.cargoforgerc <<EOF
format=table
color=yes
show_viz=yes
EOF

# Project config
echo "format=json" > .cargoforgerc

# Use example as template
cp examples/.cargoforgerc.example ~/.cargoforgerc
nano ~/.cargoforgerc
```

---

## Stdin/Stdout Operations

### Reading from Stdin

Use `-` as filename to read from stdin:

```bash
# Read cargo from pipe
cat cargo.txt | cargoforge optimize ship.cfg -

# Read ship from pipe (less common)
cat ship.cfg | cargoforge optimize - cargo.txt
```

### Chaining Commands

```bash
# Optimize and analyze in one pipeline
cargoforge optimize ship.cfg cargo.txt --format=json | \
  cargoforge analyze -

# Save intermediate results
cargoforge optimize ship.cfg cargo.txt --format=json | \
  tee results.json | \
  cargoforge analyze -
```

### Filtering Input

```bash
# Process only specific cargo types
grep "Container" cargo.txt | cargoforge optimize ship.cfg -

# Exclude hazardous cargo
grep -v "hazardous" cargo.txt | cargoforge optimize ship.cfg -
```

### Output Redirection

```bash
# Save to file
cargoforge optimize ship.cfg cargo.txt --format=json > results.json

# Append to log
cargoforge optimize ship.cfg cargo.txt 2>> optimization.log

# Separate stdout and stderr
cargoforge optimize ship.cfg cargo.txt 1>output.txt 2>errors.log
```

### Advanced Pipelines

```bash
# Multi-stage processing
cat raw_cargo.txt | \
  grep -v "^#" | \
  sort -k2 -n -r | \
  cargoforge optimize ship.cfg - --format=csv | \
  csvstat

# Batch processing
for cargo in cargos/*.txt; do
  cargoforge optimize ship.cfg "$cargo" --format=json \
    --output="results/$(basename $cargo .txt).json"
done
```

---

## Advanced Usage

### Batch Processing

```bash
# Process multiple cargo files
for cargo in *.txt; do
  echo "Processing $cargo..."
  cargoforge optimize ship.cfg "$cargo" --quiet \
    --output="${cargo%.txt}_results.json" --format=json
done
```

### Conditional Execution

```bash
# Validate before optimization
if cargoforge validate ship.cfg cargo.txt --quiet; then
  cargoforge optimize ship.cfg cargo.txt
else
  echo "Validation failed!"
  exit 1
fi
```

### Error Handling

```bash
# Capture exit codes
if ! cargoforge optimize ship.cfg cargo.txt --quiet; then
  echo "Optimization failed with code $?"
  # Send alert, log error, etc.
fi
```

### Scripting Integration

```bash
#!/bin/bash
# optimize_and_report.sh

SHIP=$1
CARGO=$2
OUTPUT="report_$(date +%Y%m%d).md"

# Run optimization
cargoforge optimize "$SHIP" "$CARGO" \
  --format=markdown \
  --output="$OUTPUT" \
  --quiet

# Check results
if [ $? -eq 0 ]; then
  echo "Report generated: $OUTPUT"
  # Email report, upload to S3, etc.
else
  echo "Optimization failed"
  exit 1
fi
```

### Integration with Other Tools

```bash
# With jq (JSON parsing)
cargoforge optimize ship.cfg cargo.txt --format=json | \
  jq '.analysis.metacentric_height'

# With csvkit (CSV analysis)
cargoforge optimize ship.cfg cargo.txt --format=csv | \
  csvstat --mean

# With watch (continuous monitoring)
watch -n 60 'cargoforge optimize ship.cfg live_cargo.txt --format=table'

# With parallel (parallel processing)
parallel cargoforge optimize ship.cfg {} --quiet ::: cargo*.txt
```

---

## Troubleshooting

### Common Issues

#### 1. "Command not found"

**Problem**: Shell can't find `cargoforge` executable

**Solutions**:
```bash
# Use full path
/home/user/CargoForge-C/cargoforge optimize ...

# Or add to PATH
export PATH=$PATH:/home/user/CargoForge-C

# Or install system-wide
sudo cp cargoforge /usr/local/bin/
```

#### 2. "Failed to parse ship config"

**Problem**: Invalid ship configuration file

**Solutions**:
```bash
# Validate file first
cargoforge validate ship.cfg cargo.txt --verbose

# Check file format
cat ship.cfg
# Should be key=value format:
# length_m=150
# width_m=25
```

#### 3. "Error: Cannot open output file"

**Problem**: No write permission or invalid path

**Solutions**:
```bash
# Check directory permissions
ls -ld $(dirname output.json)

# Create directory if needed
mkdir -p results/
cargoforge optimize ship.cfg cargo.txt --output=results/out.json --format=json
```

#### 4. Colors not working

**Problem**: Terminal doesn't support ANSI colors

**Solutions**:
```bash
# Force color off
cargoforge optimize ship.cfg cargo.txt --no-color

# Or use config file
echo "color=no" > ~/.cargoforgerc
```

#### 5. Stdin not working

**Problem**: Pipeline doesn't work with `-`

**Solutions**:
```bash
# Make sure to use - not just missing argument
cat cargo.txt | cargoforge optimize ship.cfg -

# NOT THIS (will fail):
cat cargo.txt | cargoforge optimize ship.cfg
```

### Getting Help

```bash
# General help
cargoforge --help

# Subcommand help
cargoforge optimize --help

# Version info
cargoforge --version

# Verbose mode for debugging
cargoforge optimize ship.cfg cargo.txt --verbose
```

### Reporting Bugs

Include in bug reports:
1. CargoForge-C version (`cargoforge --version`)
2. Command that failed (full command line)
3. Error messages (including stderr output)
4. Input files (if possible to share)
5. Operating system and compiler version

---

## Appendix

### Input File Formats

#### Ship Configuration (.cfg)

```ini
# Ship specifications (key=value format)
length_m=150
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0
```

#### Cargo Manifest (.txt)

```
# Cargo manifest (space/tab separated)
# ID  Weight(tonnes)  Dimensions(LxWxH)  Type

HeavyMachinery    550    20x5x3        standard
ContainerA        250    12.2x2.4x2.6  reefer
HazmatCargo       100    10x3x3        hazardous
FragileGoods      50     5x5x5         fragile
BulkCargo         400    18x2x2        bulk
```

**Cargo Types**:
- `standard` - Regular cargo
- `hazardous` - Hazmat (requires 3m separation)
- `reefer` - Refrigerated cargo
- `fragile` - Fragile items (stacking restrictions)
- `bulk` - Bulk cargo
- `general` - General goods

### Exit Codes Reference

| Code | Meaning | Constant |
|------|---------|----------|
| 0 | Success | EXIT_SUCCESS |
| 1 | Invalid arguments | EXIT_INVALID_ARGS |
| 2 | File error (not found, no permission) | EXIT_FILE_ERROR |
| 3 | Parse error (invalid format) | EXIT_PARSE_ERROR |
| 4 | Optimization error | EXIT_OPTIMIZATION_ERROR |
| 5 | Validation error | EXIT_VALIDATION_ERROR |

### Environment Variables

Currently CargoForge-C only uses standard environment variables:

- `HOME` - For locating `~/.cargoforgerc`
- `TERM` - For color support detection

### Performance Tips

1. **Use quiet mode in scripts**: `--quiet` reduces I/O overhead
2. **JSON is faster than tables**: For programmatic use
3. **Disable visualization**: `--no-viz` speeds up output
4. **Use config files**: Reduces argument parsing time
5. **Batch similar operations**: Process multiple files in one session

---

**End of CLI Guide**

For more information:
- GitHub: https://github.com/alikatgh/CargoForge-C
- Issues: https://github.com/alikatgh/CargoForge-C/issues
- Examples: See `examples/` directory
