# CargoForge-C Examples

**Real-World Usage Examples and Recipes**

This guide provides practical examples for common CargoForge-C workflows.

---

## Table of Contents

1. [Basic Examples](#basic-examples)
2. [Output Formats](#output-formats)
3. [Pipelines and Chaining](#pipelines-and-chaining)
4. [Batch Processing](#batch-processing)
5. [Automation Scripts](#automation-scripts)
6. [Integration Examples](#integration-examples)
7. [Advanced Workflows](#advanced-workflows)

---

## Basic Examples

### Example 1: First Optimization

```bash
# Run your first optimization
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

**Output**:
```
CargoForge-C Optimizer v2.1.0
Ship: examples/sample_ship.cfg
Cargo: examples/sample_cargo.txt

✓ Ship configuration loaded
✓ Cargo manifest loaded

Running optimization algorithm...
✓ Optimization complete

--- CargoForge Stability Analysis ---

Ship Specs: 150.00 m × 25.00 m | Max Weight: 50000.00 t
----------------------------------------------------------------------
  - HeavyMachinery  | Pos (X,Y): (   0.00,    0.00) | 550.00 t
  - ContainerA      | Pos (X,Y): (   0.00,    0.00) | 250.00 t
  - ContainerB      | Pos (X,Y): (   0.00,    2.40) | 250.00 t
  ...
```

---

### Example 2: Validation First

```bash
# Always validate before optimization in production
./cargoforge validate examples/sample_ship.cfg examples/sample_cargo.txt

# If valid, proceed with optimization
./cargoforge validate ship.cfg cargo.txt && \
  ./cargoforge optimize ship.cfg cargo.txt
```

**Output**:
```
CargoForge-C Validator
======================

Validating ship configuration: ship.cfg
✓ Ship configuration is valid
  Length: 150.00 m
  Width: 25.00 m
  Max weight: 50000000 kg

Validating cargo manifest: cargo.txt
✓ Cargo manifest is valid
  Total items: 5
  Total weight: 1500000 kg

✓ All validation checks passed!
```

---

### Example 3: Get Ship Info

```bash
# Display ship specifications
./cargoforge info examples/sample_ship.cfg
```

**Output**:
```
Ship Information
================

Dimensions:
  Length: 150.00 m
  Width: 25.00 m
  Deck Area: 3750.00 m²

Weight Capacity:
  Maximum cargo: 50000.00 tonnes
  Lightship weight: 2000.00 tonnes

Stability:
  Lightship KG: 8.00 m
```

---

## Output Formats

### Example 4: JSON for API

```bash
# Generate JSON output
./cargoforge optimize ship.cfg cargo.txt --format=json --quiet
```

**Output** (formatted for readability):
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
    "metacentric_height": 47.06
  }
}
```

---

### Example 5: CSV for Spreadsheets

```bash
# Export to CSV
./cargoforge optimize ship.cfg cargo.txt --format=csv > results.csv
```

**results.csv**:
```csv
ID,Weight_kg,Length_m,Width_m,Height_m,Type,Placed,Pos_X,Pos_Y,Pos_Z
HeavyMachinery,550000.00,20.00,5.00,3.00,standard,yes,0.00,0.00,-8.00
ContainerA,250000.00,12.20,2.40,2.60,reefer,yes,0.00,0.00,-5.00
ContainerB,250000.00,12.20,2.40,2.60,reefer,yes,0.00,2.40,-5.00
```

Open in Excel:
```bash
libreoffice results.csv
```

---

### Example 6: Markdown Reports

```bash
# Generate markdown report
./cargoforge optimize ship.cfg cargo.txt --format=markdown > REPORT.md
```

**REPORT.md**:
```markdown
# CargoForge-C Optimization Report

## Ship Specifications

| Property | Value |
|----------|-------|
| Length | 150.00 m |
| Width | 25.00 m |
| Max Weight | 50000 tonnes |

## Cargo Placement Results

| Cargo ID | Weight (t) | Dimensions | Type | Status | Position |
|----------|------------|------------|------|--------|----------|
| HeavyMachinery | 550.00 | 20.0×5.0×3.0 | standard | ✅ Placed | (0.0, 0.0, -8.0) |
```

---

### Example 7: Professional Tables

```bash
# Display as table
./cargoforge optimize ship.cfg cargo.txt --format=table
```

**Output**:
```
┌──────────────────┬───────────┬────────────────────┬──────────────┬─────────┬────────────────────────┐
│ Cargo ID         │ Weight(t) │ Dimensions (LxWxH) │ Type         │ Placed  │ Position (X,Y,Z)       │
├──────────────────┼───────────┼────────────────────┼──────────────┼─────────┼────────────────────────┤
│ HeavyMachinery   │    550.00 │ 20.0x5.0x3.0        │ standard     │ YES     │ (0.0, 0.0, -8.0)       │
│ ContainerA       │    250.00 │ 12.2x2.4x2.6        │ reefer       │ YES     │ (0.0, 0.0, -5.0)       │
└──────────────────┴───────────┴────────────────────┴──────────────┴─────────┴────────────────────────┘
```

---

## Pipelines and Chaining

### Example 8: Stdin Input

```bash
# Read cargo from pipe
cat cargo.txt | ./cargoforge optimize ship.cfg -
```

```bash
# Filter cargo before optimization
grep "Container" cargo.txt | ./cargoforge optimize ship.cfg -
```

```bash
# Sort by weight (descending) and optimize
sort -k2 -n -r cargo.txt | ./cargoforge optimize ship.cfg -
```

---

### Example 9: Optimize and Analyze

```bash
# Single pipeline: optimize and analyze
./cargoforge optimize ship.cfg cargo.txt --format=json | \
  ./cargoforge analyze -
```

**Output**:
```
═══════════════════════════════════════════════════
  CargoForge-C Results Analysis
═══════════════════════════════════════════════════

SHIP SPECIFICATIONS:
  Dimensions: 150.00 m × 25.00 m
  Capacity: 50000 tonnes

CARGO SUMMARY:
  Total Items: 5
  Successfully Placed: 5 (100.0%)

STABILITY ANALYSIS:
  Metacentric Height (GM): 47.06 m
  Stability Status: ⚠ TOO STIFF

⚠ RECOMMENDATIONS:
  • Consider lowering center of gravity
```

---

### Example 10: Save and Analyze

```bash
# Save results for later analysis
./cargoforge optimize ship.cfg cargo.txt \
  --format=json \
  --output=results.json \
  --quiet

# Analyze saved results
./cargoforge analyze results.json
```

---

### Example 11: Tee for Multiple Outputs

```bash
# Optimize once, analyze and save
./cargoforge optimize ship.cfg cargo.txt --format=json | \
  tee results.json | \
  ./cargoforge analyze -
```

This saves results.json AND displays analysis!

---

## Batch Processing

### Example 12: Process Multiple Cargo Files

```bash
# Process all cargo files in directory
for cargo in cargos/*.txt; do
  echo "Processing $(basename $cargo)..."
  ./cargoforge optimize ship.cfg "$cargo" --quiet \
    --format=json \
    --output="results/$(basename $cargo .txt).json"
done
```

---

### Example 13: Parallel Processing

```bash
# Install GNU parallel first
# sudo apt-get install parallel

# Process files in parallel
parallel ./cargoforge optimize ship.cfg {} --quiet --format=json \
  --output=results/{/.}.json ::: cargos/*.txt
```

---

### Example 14: Batch Validation

```bash
# Validate multiple configurations
#!/bin/bash
failed=0
for cargo in *.txt; do
  if ! ./cargoforge validate ship.cfg "$cargo" --quiet 2>/dev/null; then
    echo "FAILED: $cargo"
    ((failed++))
  fi
done

if [ $failed -gt 0 ]; then
  echo "$failed file(s) failed validation"
  exit 1
else
  echo "All files validated successfully!"
fi
```

---

## Automation Scripts

### Example 15: Daily Optimization Report

```bash
#!/bin/bash
# daily_report.sh

DATE=$(date +%Y-%m-%d)
REPORT="report_${DATE}.md"

echo "Generating daily optimization report..."

./cargoforge optimize ship.cfg daily_cargo.txt \
  --format=markdown \
  --output="$REPORT" \
  --quiet

if [ $? -eq 0 ]; then
  echo "Report generated: $REPORT"

  # Email report
  mail -s "Daily Cargo Report ${DATE}" \
    operations@company.com < "$REPORT"

  # Upload to cloud
  aws s3 cp "$REPORT" "s3://cargo-reports/${REPORT}"

  echo "Report sent and uploaded"
else
  echo "ERROR: Report generation failed"
  exit 1
fi
```

---

### Example 16: Continuous Integration

```bash
#!/bin/bash
# ci_test.sh - Run in CI pipeline

set -e  # Exit on error

echo "Running CargoForge-C tests..."

# Test 1: Validation
echo "Test 1: Validation..."
./cargoforge validate ship.cfg cargo.txt --quiet
echo "✓ Validation passed"

# Test 2: Optimization
echo "Test 2: Optimization..."
./cargoforge optimize ship.cfg cargo.txt --quiet --format=json > results.json
echo "✓ Optimization passed"

# Test 3: Analysis
echo "Test 3: Analysis..."
./cargoforge analyze results.json --quiet > /dev/null
echo "✓ Analysis passed"

# Test 4: Check JSON structure
echo "Test 4: JSON validation..."
jq -e '.analysis.placed_count >= 0' results.json > /dev/null
echo "✓ JSON structure valid"

echo "All tests passed!"
```

---

### Example 17: Error Alerting

```bash
#!/bin/bash
# optimize_with_alerts.sh

SHIP=$1
CARGO=$2

# Run optimization
if ! ./cargoforge optimize "$SHIP" "$CARGO" --quiet --format=json > results.json; then
  # Send alert on failure
  curl -X POST https://hooks.slack.com/services/YOUR/WEBHOOK/URL \
    -H 'Content-Type: application/json' \
    -d '{
      "text": "❌ CargoForge optimization failed!",
      "attachments": [{
        "color": "danger",
        "fields": [
          {"title": "Ship", "value": "'"$SHIP"'", "short": true},
          {"title": "Cargo", "value": "'"$CARGO"'", "short": true}
        ]
      }]
    }'

  exit 1
fi

# Check stability
GM=$(jq -r '.analysis.metacentric_height' results.json)

if (( $(echo "$GM < 0.5" | bc -l) )); then
  # Send warning
  curl -X POST https://hooks.slack.com/services/YOUR/WEBHOOK/URL \
    -H 'Content-Type: application/json' \
    -d '{
      "text": "⚠️ Unstable cargo configuration detected!",
      "attachments": [{
        "color": "warning",
        "fields": [
          {"title": "GM", "value": "'"$GM"' m", "short": true},
          {"title": "Status", "value": "UNSTABLE", "short": true}
        ]
      }]
    }'
fi
```

---

## Integration Examples

### Example 18: Python Integration

```python
#!/usr/bin/env python3
import subprocess
import json
import sys

def optimize_cargo(ship_file, cargo_file):
    """Run CargoForge optimization from Python"""

    cmd = [
        './cargoforge', 'optimize',
        ship_file, cargo_file,
        '--format=json',
        '--quiet'
    ]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=True
        )

        data = json.loads(result.stdout)
        return data

    except subprocess.CalledProcessError as e:
        print(f"Error: {e.stderr}", file=sys.stderr)
        return None

# Usage
if __name__ == '__main__':
    result = optimize_cargo('ship.cfg', 'cargo.txt')

    if result:
        analysis = result['analysis']
        print(f"Placed: {analysis['placed_count']}/{analysis['total_count']}")
        print(f"GM: {analysis['metacentric_height']:.2f} m")
        print(f"Status: {analysis['stability_status']}")
```

---

### Example 19: Node.js Integration

```javascript
// optimize.js
const { exec } = require('child_process');
const util = require('util');
const execPromise = util.promisify(exec);

async function optimizeCargo(shipFile, cargoFile) {
  const cmd = `./cargoforge optimize ${shipFile} ${cargoFile} --format=json --quiet`;

  try {
    const { stdout, stderr } = await execPromise(cmd);

    if (stderr && !stderr.includes('Note:')) {
      console.error('Warning:', stderr);
    }

    const result = JSON.parse(stdout);
    return result;

  } catch (error) {
    console.error('Optimization failed:', error.message);
    throw error;
  }
}

// Usage
(async () => {
  try {
    const result = await optimizeCargo('ship.cfg', 'cargo.txt');
    console.log('Placed:', result.analysis.placed_count);
    console.log('GM:', result.analysis.metacentric_height);
  } catch (error) {
    process.exit(1);
  }
})();
```

---

### Example 20: REST API Wrapper

```python
#!/usr/bin/env python3
from flask import Flask, request, jsonify
import subprocess
import tempfile
import os
import json

app = Flask(__name__)

@app.route('/api/optimize', methods=['POST'])
def optimize():
    """API endpoint for cargo optimization"""
    data = request.json

    # Create temp files
    with tempfile.NamedTemporaryFile(mode='w', suffix='.cfg', delete=False) as ship_file:
        ship_file.write(f"length_m={data['ship']['length']}\n")
        ship_file.write(f"width_m={data['ship']['width']}\n")
        ship_file.write(f"max_weight_tonnes={data['ship']['max_weight']}\n")
        ship_file.write(f"lightship_weight_tonnes={data['ship']['lightship_weight']}\n")
        ship_file.write(f"lightship_kg_m={data['ship']['lightship_kg']}\n")
        ship_path = ship_file.name

    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as cargo_file:
        for item in data['cargo']:
            dims = f"{item['length']}x{item['width']}x{item['height']}"
            cargo_file.write(f"{item['id']} {item['weight']} {dims} {item['type']}\n")
        cargo_path = cargo_file.name

    try:
        # Run optimization
        result = subprocess.run(
            ['./cargoforge', 'optimize', ship_path, cargo_path, '--format=json', '--quiet'],
            capture_output=True,
            text=True,
            check=True
        )

        # Parse and return results
        optimization_result = json.loads(result.stdout)
        return jsonify(optimization_result)

    except subprocess.CalledProcessError as e:
        return jsonify({'error': e.stderr}), 500

    finally:
        # Cleanup temp files
        os.unlink(ship_path)
        os.unlink(cargo_path)

if __name__ == '__main__':
    app.run(debug=True, port=5000)
```

---

## Advanced Workflows

### Example 21: Configuration Management

```bash
# Development environment
cat > .cargoforgerc <<EOF
format=table
color=yes
verbose=yes
show_viz=yes
EOF

# Production environment
cat > .cargoforgerc <<EOF
format=json
color=no
quiet=yes
show_viz=no
EOF
```

---

### Example 22: Multi-Stage Pipeline

```bash
#!/bin/bash
# multi_stage.sh

set -e

echo "Stage 1: Validation"
./cargoforge validate ship.cfg cargo.txt --verbose

echo "Stage 2: Optimization"
./cargoforge optimize ship.cfg cargo.txt \
  --format=json \
  --output=results.json \
  --quiet

echo "Stage 3: Analysis"
./cargoforge analyze results.json > analysis.txt

echo "Stage 4: Report Generation"
./cargoforge optimize ship.cfg cargo.txt \
  --format=markdown \
  --output=report.md \
  --quiet

echo "Stage 5: Data Export"
./cargoforge optimize ship.cfg cargo.txt \
  --format=csv \
  --output=data.csv \
  --quiet

echo "Pipeline complete!"
echo "Generated files:"
ls -lh results.json analysis.txt report.md data.csv
```

---

### Example 23: Watch Mode (Auto-Rerun)

```bash
# Install fswatch first
# brew install fswatch  (macOS)
# sudo apt-get install inotify-tools  (Linux)

# Auto-run on file changes
fswatch -o cargo.txt | while read; do
  clear
  echo "File changed, re-running optimization..."
  ./cargoforge optimize ship.cfg cargo.txt --format=table
done
```

**Or with inotify-tools**:
```bash
while inotifywait -e modify cargo.txt; do
  ./cargoforge optimize ship.cfg cargo.txt --format=table
done
```

---

### Example 24: Comparison Testing

```bash
#!/bin/bash
# compare_algorithms.sh

echo "Comparing 3D vs 2D algorithms..."

# Run 3D
./cargoforge optimize ship.cfg cargo.txt \
  --algorithm=3d \
  --format=json \
  --output=results_3d.json \
  --quiet

# Run 2D
./cargoforge optimize ship.cfg cargo.txt \
  --algorithm=2d \
  --format=json \
  --output=results_2d.json \
  --quiet

# Compare results
echo "3D Algorithm:"
jq '.analysis | {placed: .placed_count, gm: .metacentric_height}' results_3d.json

echo "2D Algorithm:"
jq '.analysis | {placed: .placed_count, gm: .metacentric_height}' results_2d.json
```

---

### Example 25: Docker Integration

```dockerfile
# Dockerfile
FROM gcc:12

WORKDIR /app
COPY . .

RUN make clean && make

ENTRYPOINT ["./cargoforge"]
CMD ["--help"]
```

**Build and run**:
```bash
# Build image
docker build -t cargoforge .

# Run optimization
docker run -v $(pwd)/data:/data cargoforge \
  optimize /data/ship.cfg /data/cargo.txt --format=json
```

---

## Tips and Tricks

### Quick Reference

```bash
# Fastest validation
./cargoforge validate ship.cfg cargo.txt --quiet && echo "OK" || echo "FAILED"

# Silent optimization for scripts
./cargoforge optimize ship.cfg cargo.txt --quiet --format=json 2>/dev/null

# Debug mode
./cargoforge optimize ship.cfg cargo.txt --verbose --no-color 2>&1 | tee debug.log

# One-liner analysis
./cargoforge optimize ship.cfg cargo.txt --format=json | \
  jq -r '.analysis | "\(.placed_count)/\(.total_count) placed, GM: \(.metacentric_height)m"'
```

---

**End of Examples**

For more information, see:
- [CLI Guide](CLI_GUIDE.md)
- [GitHub Repository](https://github.com/alikatgh/CargoForge-C)
