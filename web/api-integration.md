# API Integration

CargoForge-C integrates with any language via subprocess execution and JSON output.

## Python

```python
import subprocess, json

class CargoForge:
    def __init__(self, binary='./cargoforge'):
        self.binary = binary

    def optimize(self, ship_file, cargo_file):
        result = subprocess.run(
            [self.binary, 'optimize', ship_file, cargo_file, '--format=json', '--quiet'],
            capture_output=True, text=True, check=True
        )
        return json.loads(result.stdout)

    def validate(self, ship_file, cargo_file):
        result = subprocess.run(
            [self.binary, 'validate', ship_file, cargo_file, '--quiet'],
            capture_output=True
        )
        return result.returncode == 0

cf = CargoForge()
if cf.validate('ship.cfg', 'cargo.txt'):
    data = cf.optimize('ship.cfg', 'cargo.txt')
    print(f"GM: {data['analysis']['metacentric_height']:.2f} m")
```

## Node.js

```javascript
const { execSync } = require('child_process');

function optimize(shipFile, cargoFile) {
  const out = execSync(
    `./cargoforge optimize ${shipFile} ${cargoFile} --format=json --quiet`,
    { encoding: 'utf-8' }
  );
  return JSON.parse(out);
}

const result = optimize('ship.cfg', 'cargo.txt');
console.log('Placed:', result.analysis.placed_count);
```

## REST API (Flask)

```python
from flask import Flask, request, jsonify
import subprocess, tempfile, os, json

app = Flask(__name__)

@app.route('/api/optimize', methods=['POST'])
def optimize():
    data = request.json

    with tempfile.NamedTemporaryFile(mode='w', suffix='.cfg', delete=False) as f:
        f.write(f"length_m={data['ship']['length']}\n")
        f.write(f"width_m={data['ship']['width']}\n")
        f.write(f"max_weight_tonnes={data['ship']['max_weight']}\n")
        f.write(f"lightship_weight_tonnes={data['ship']['lightship_weight']}\n")
        f.write(f"lightship_kg_m={data['ship']['lightship_kg']}\n")
        ship_path = f.name

    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
        for item in data['cargo']:
            dims = f"{item['length']}x{item['width']}x{item['height']}"
            f.write(f"{item['id']} {item['weight']} {dims} {item['type']}\n")
        cargo_path = f.name

    try:
        result = subprocess.run(
            ['./cargoforge', 'optimize', ship_path, cargo_path, '--format=json', '--quiet'],
            capture_output=True, text=True, check=True
        )
        return jsonify(json.loads(result.stdout))
    except subprocess.CalledProcessError as e:
        return jsonify({'error': e.stderr}), 500
    finally:
        os.unlink(ship_path)
        os.unlink(cargo_path)

if __name__ == '__main__':
    app.run(port=5000)
```

## Go

```go
package main

import (
    "encoding/json"
    "fmt"
    "os/exec"
)

type Result struct {
    Analysis struct {
        PlacedCount       int     `json:"placed_count"`
        MetacentricHeight float64 `json:"metacentric_height"`
    } `json:"analysis"`
}

func main() {
    out, err := exec.Command(
        "./cargoforge", "optimize", "ship.cfg", "cargo.txt",
        "--format=json", "--quiet",
    ).Output()
    if err != nil {
        panic(err)
    }

    var r Result
    json.Unmarshal(out, &r)
    fmt.Printf("Placed: %d, GM: %.2f m\n", r.Analysis.PlacedCount, r.Analysis.MetacentricHeight)
}
```

## Best Practices

1. **Validate first** — Always call `validate` before `optimize`
2. **Use `--quiet`** — Suppresses stderr noise for clean JSON parsing
3. **Set timeouts** — `subprocess.run(..., timeout=30)` to prevent hangs
4. **Clean up temp files** — Use `try/finally` or context managers
5. **Cache results** — Hash input files to avoid redundant runs
