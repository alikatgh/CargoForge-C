# API Integration Guide

**Integrating CargoForge-C with Applications and Services**

This guide shows how to integrate CargoForge-C into your applications, web services, and automation workflows.

---

## Table of Contents

1. [Integration Approaches](#integration-approaches)
2. [Python Integration](#python-integration)
3. [Node.js Integration](#nodejs-integration)
4. [REST API Wrapper](#rest-api-wrapper)
5. [PHP Integration](#php-integration)
6. [Ruby Integration](#ruby-integration)
7. [Go Integration](#go-integration)
8. [Best Practices](#best-practices)

---

## Integration Approaches

### 1. Subprocess Execution

**Pros**:
- Simple to implement
- Language-agnostic
- No dependencies

**Cons**:
- Process overhead
- Error handling complexity

**Best for**: Background jobs, batch processing

---

### 2. REST API Wrapper

**Pros**:
- Network-accessible
- Multiple clients
- Familiar HTTP interface

**Cons**:
- Additional server needed
- Network latency

**Best for**: Web applications, microservices

---

### 3. Shared Library (Future)

**Pros**:
- Direct function calls
- No process overhead
- Maximum performance

**Cons**:
- Language bindings needed
- More complex integration

**Best for**: High-performance applications (planned feature)

---

## Python Integration

### Basic Example

```python
#!/usr/bin/env python3
"""
CargoForge-C Python Integration
"""
import subprocess
import json
from typing import Dict, Optional

class CargoForge:
    def __init__(self, binary_path='./cargoforge'):
        self.binary = binary_path

    def optimize(
        self,
        ship_file: str,
        cargo_file: str,
        output_format: str = 'json',
        quiet: bool = True
    ) -> Optional[Dict]:
        """
        Run cargo optimization

        Args:
            ship_file: Path to ship configuration
            cargo_file: Path to cargo manifest
            output_format: Output format (json, csv, table, markdown)
            quiet: Suppress stderr output

        Returns:
            Dictionary with optimization results (if format=json)
            None on error
        """
        cmd = [
            self.binary,
            'optimize',
            ship_file,
            cargo_file,
            f'--format={output_format}'
        ]

        if quiet:
            cmd.append('--quiet')

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True
            )

            if output_format == 'json':
                return json.loads(result.stdout)
            else:
                return {'output': result.stdout}

        except subprocess.CalledProcessError as e:
            print(f"Error: {e.stderr}")
            return None

    def validate(self, ship_file: str, cargo_file: str) -> bool:
        """
        Validate input files

        Returns:
            True if valid, False otherwise
        """
        cmd = [
            self.binary,
            'validate',
            ship_file,
            cargo_file,
            '--quiet'
        ]

        result = subprocess.run(cmd, capture_output=True)
        return result.returncode == 0

    def analyze(self, results_file: str) -> Optional[str]:
        """
        Analyze JSON results

        Returns:
            Analysis text or None on error
        """
        cmd = [
            self.binary,
            'analyze',
            results_file,
            '--quiet'
        ]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True
            )
            return result.stdout

        except subprocess.CalledProcessError as e:
            print(f"Error: {e.stderr}")
            return None


# Usage Example
if __name__ == '__main__':
    cf = CargoForge()

    # Validate first
    if not cf.validate('ship.cfg', 'cargo.txt'):
        print("Validation failed!")
        exit(1)

    # Run optimization
    result = cf.optimize('ship.cfg', 'cargo.txt')

    if result:
        analysis = result['analysis']
        print(f"Placed: {analysis['placed_count']}/{analysis['total_count']}")
        print(f"GM: {analysis['metacentric_height']:.2f} m")
        print(f"Status: {analysis['stability_status']}")

        # Save results
        with open('results.json', 'w') as f:
            json.dump(result, f, indent=2)
```

### Advanced Example with Context Manager

```python
import tempfile
import os
from contextlib import contextmanager

class CargoForgeContext:
    def __init__(self):
        self.cf = CargoForge()

    @contextmanager
    def create_ship(self, length, width, max_weight):
        """Create temporary ship config"""
        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.cfg',
            delete=False
        ) as f:
            f.write(f"length_m={length}\n")
            f.write(f"width_m={width}\n")
            f.write(f"max_weight_tonnes={max_weight}\n")
            f.write(f"lightship_weight_tonnes={max_weight * 0.1}\n")
            f.write(f"lightship_kg_m={length / 2}\n")
            ship_file = f.name

        try:
            yield ship_file
        finally:
            os.unlink(ship_file)

    @contextmanager
    def create_cargo(self, cargo_items):
        """Create temporary cargo manifest"""
        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.txt',
            delete=False
        ) as f:
            for item in cargo_items:
                dims = f"{item['l']}x{item['w']}x{item['h']}"
                f.write(f"{item['id']} {item['weight']} {dims} {item['type']}\n")
            cargo_file = f.name

        try:
            yield cargo_file
        finally:
            os.unlink(cargo_file)

# Usage
cargo_items = [
    {'id': 'Container1', 'weight': 250, 'l': 12, 'w': 2.4, 'h': 2.6, 'type': 'standard'},
    {'id': 'Container2', 'weight': 250, 'l': 12, 'w': 2.4, 'h': 2.6, 'type': 'reefer'}
]

ctx = CargoForgeContext()

with ctx.create_ship(150, 25, 50000) as ship_file:
    with ctx.create_cargo(cargo_items) as cargo_file:
        result = ctx.cf.optimize(ship_file, cargo_file)
        print(result['analysis'])
```

---

## Node.js Integration

### Basic Module

```javascript
// cargoforge.js
const { exec } = require('child_process');
const util = require('util');
const fs = require('fs').promises;
const execPromise = util.promisify(exec);

class CargoForge {
  constructor(binaryPath = './cargoforge') {
    this.binary = binaryPath;
  }

  async optimize(shipFile, cargoFile, options = {}) {
    const format = options.format || 'json';
    const quiet = options.quiet !== false;

    const cmd = [
      this.binary,
      'optimize',
      shipFile,
      cargoFile,
      `--format=${format}`,
      quiet ? '--quiet' : ''
    ].filter(Boolean).join(' ');

    try {
      const { stdout, stderr } = await execPromise(cmd);

      // Filter out informational messages
      if (stderr && !stderr.includes('Note:')) {
        console.warn('Warning:', stderr);
      }

      if (format === 'json') {
        return JSON.parse(stdout);
      }
      return { output: stdout };

    } catch (error) {
      throw new Error(`Optimization failed: ${error.message}`);
    }
  }

  async validate(shipFile, cargoFile) {
    const cmd = `${this.binary} validate ${shipFile} ${cargoFile} --quiet`;

    try {
      await execPromise(cmd);
      return true;
    } catch (error) {
      return false;
    }
  }

  async analyze(resultsFile) {
    const cmd = `${this.binary} analyze ${resultsFile} --quiet`;

    try {
      const { stdout } = await execPromise(cmd);
      return stdout;
    } catch (error) {
      throw new Error(`Analysis failed: ${error.message}`);
    }
  }

  async createShipConfig(params) {
    const content = `
length_m=${params.length}
width_m=${params.width}
max_weight_tonnes=${params.maxWeight}
lightship_weight_tonnes=${params.lightshipWeight || params.maxWeight * 0.1}
lightship_kg_m=${params.lightshipKG || params.length / 2}
`.trim();

    const tmpFile = `/tmp/ship_${Date.now()}.cfg`;
    await fs.writeFile(tmpFile, content);
    return tmpFile;
  }

  async createCargoManifest(items) {
    const lines = items.map(item => {
      const dims = `${item.length}x${item.width}x${item.height}`;
      return `${item.id} ${item.weight} ${dims} ${item.type}`;
    });

    const tmpFile = `/tmp/cargo_${Date.now()}.txt`;
    await fs.writeFile(tmpFile, lines.join('\n'));
    return tmpFile;
  }
}

module.exports = CargoForge;
```

### Usage Example

```javascript
// example.js
const CargoForge = require('./cargoforge');

async function main() {
  const cf = new CargoForge();

  try {
    // Validate
    const isValid = await cf.validate('ship.cfg', 'cargo.txt');
    if (!isValid) {
      console.error('Validation failed!');
      return;
    }

    // Optimize
    const result = await cf.optimize('ship.cfg', 'cargo.txt');

    console.log(`Placed: ${result.analysis.placed_count}/${result.analysis.total_count}`);
    console.log(`GM: ${result.analysis.metacentric_height} m`);

    // Save results
    await fs.writeFile('results.json', JSON.stringify(result, null, 2));

  } catch (error) {
    console.error('Error:', error.message);
    process.exit(1);
  }
}

main();
```

### Express.js REST API

```javascript
// api.js
const express = require('express');
const CargoForge = require('./cargoforge');
const fs = require('fs').promises;

const app = express();
const cf = new CargoForge();

app.use(express.json());

// POST /api/optimize
app.post('/api/optimize', async (req, res) => {
  try {
    const { ship, cargo } = req.body;

    // Create temp files
    const shipFile = await cf.createShipConfig(ship);
    const cargoFile = await cf.createCargoManifest(cargo);

    // Run optimization
    const result = await cf.optimize(shipFile, cargoFile);

    // Cleanup
    await fs.unlink(shipFile);
    await fs.unlink(cargoFile);

    res.json(result);

  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// POST /api/validate
app.post('/api/validate', async (req, res) => {
  try {
    const { ship, cargo } = req.body;

    const shipFile = await cf.createShipConfig(ship);
    const cargoFile = await cf.createCargoManifest(cargo);

    const isValid = await cf.validate(shipFile, cargoFile);

    await fs.unlink(shipFile);
    await fs.unlink(cargoFile);

    res.json({ valid: isValid });

  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

app.listen(3000, () => {
  console.log('CargoForge API running on port 3000');
});
```

---

## REST API Wrapper

### Flask (Python)

```python
# api.py
from flask import Flask, request, jsonify
import subprocess
import tempfile
import os
import json

app = Flask(__name__)

@app.route('/api/optimize', methods=['POST'])
def optimize():
    data = request.json

    # Create temp ship config
    ship_fd, ship_path = tempfile.mkstemp(suffix='.cfg', text=True)
    with os.fdopen(ship_fd, 'w') as f:
        f.write(f"length_m={data['ship']['length']}\n")
        f.write(f"width_m={data['ship']['width']}\n")
        f.write(f"max_weight_tonnes={data['ship']['max_weight']}\n")
        f.write(f"lightship_weight_tonnes={data['ship'].get('lightship_weight', data['ship']['max_weight'] * 0.1)}\n")
        f.write(f"lightship_kg_m={data['ship'].get('lightship_kg', data['ship']['length'] / 2)}\n")

    # Create temp cargo manifest
    cargo_fd, cargo_path = tempfile.mkstemp(suffix='.txt', text=True)
    with os.fdopen(cargo_fd, 'w') as f:
        for item in data['cargo']:
            dims = f"{item['length']}x{item['width']}x{item['height']}"
            f.write(f"{item['id']} {item['weight']} {dims} {item['type']}\n")

    try:
        # Run optimization
        result = subprocess.run(
            ['./cargoforge', 'optimize', ship_path, cargo_path, '--format=json', '--quiet'],
            capture_output=True,
            text=True,
            check=True
        )

        return jsonify(json.loads(result.stdout))

    except subprocess.CalledProcessError as e:
        return jsonify({'error': e.stderr}), 500

    finally:
        os.unlink(ship_path)
        os.unlink(cargo_path)

@app.route('/api/validate', methods=['POST'])
def validate():
    data = request.json

    # Similar temp file creation...

    result = subprocess.run(
        ['./cargoforge', 'validate', ship_path, cargo_path, '--quiet'],
        capture_output=True
    )

    os.unlink(ship_path)
    os.unlink(cargo_path)

    return jsonify({'valid': result.returncode == 0})

if __name__ == '__main__':
    app.run(debug=True, port=5000)
```

### FastAPI (Python - Async)

```python
# api_async.py
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import asyncio
import tempfile
import os
import json
from typing import List

app = FastAPI()

class Ship(BaseModel):
    length: float
    width: float
    max_weight: float
    lightship_weight: float = None
    lightship_kg: float = None

class CargoItem(BaseModel):
    id: str
    weight: float
    length: float
    width: float
    height: float
    type: str = 'standard'

class OptimizeRequest(BaseModel):
    ship: Ship
    cargo: List[CargoItem]

@app.post('/api/optimize')
async def optimize(request: OptimizeRequest):
    # Create temp files
    ship_fd, ship_path = tempfile.mkstemp(suffix='.cfg')
    cargo_fd, cargo_path = tempfile.mkstemp(suffix='.txt')

    try:
        # Write ship config
        with os.fdopen(ship_fd, 'w') as f:
            ship = request.ship
            f.write(f"length_m={ship.length}\n")
            f.write(f"width_m={ship.width}\n")
            f.write(f"max_weight_tonnes={ship.max_weight}\n")
            f.write(f"lightship_weight_tonnes={ship.lightship_weight or ship.max_weight * 0.1}\n")
            f.write(f"lightship_kg_m={ship.lightship_kg or ship.length / 2}\n")

        # Write cargo manifest
        with os.fdopen(cargo_fd, 'w') as f:
            for item in request.cargo:
                dims = f"{item.length}x{item.width}x{item.height}"
                f.write(f"{item.id} {item.weight} {dims} {item.type}\n")

        # Run optimization
        process = await asyncio.create_subprocess_exec(
            './cargoforge', 'optimize', ship_path, cargo_path,
            '--format=json', '--quiet',
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )

        stdout, stderr = await process.communicate()

        if process.returncode != 0:
            raise HTTPException(status_code=500, detail=stderr.decode())

        return json.loads(stdout.decode())

    finally:
        os.unlink(ship_path)
        os.unlink(cargo_path)
```

---

## PHP Integration

```php
<?php
// CargoForge.php

class CargoForge {
    private $binary;

    public function __construct($binaryPath = './cargoforge') {
        $this->binary = $binaryPath;
    }

    public function optimize($shipFile, $cargoFile, $format = 'json') {
        $cmd = sprintf(
            '%s optimize %s %s --format=%s --quiet 2>&1',
            escapeshellcmd($this->binary),
            escapeshellarg($shipFile),
            escapeshellarg($cargoFile),
            escapeshellarg($format)
        );

        $output = shell_exec($cmd);

        if ($format === 'json') {
            return json_decode($output, true);
        }

        return $output;
    }

    public function validate($shipFile, $cargoFile) {
        $cmd = sprintf(
            '%s validate %s %s --quiet 2>&1',
            escapeshellcmd($this->binary),
            escapeshellarg($shipFile),
            escapeshellarg($cargoFile)
        );

        exec($cmd, $output, $returnCode);
        return $returnCode === 0;
    }
}

// Usage
$cf = new CargoForge();

if ($cf->validate('ship.cfg', 'cargo.txt')) {
    $result = $cf->optimize('ship.cfg', 'cargo.txt');
    echo "Placed: " . $result['analysis']['placed_count'] . "\n";
    echo "GM: " . $result['analysis']['metacentric_height'] . " m\n";
}
?>
```

---

## Ruby Integration

```ruby
# cargoforge.rb
require 'json'
require 'tempfile'

class CargoForge
  def initialize(binary_path = './cargoforge')
    @binary = binary_path
  end

  def optimize(ship_file, cargo_file, format: 'json', quiet: true)
    cmd = [
      @binary,
      'optimize',
      ship_file,
      cargo_file,
      "--format=#{format}"
    ]
    cmd << '--quiet' if quiet

    output = `#{cmd.join(' ')} 2>&1`

    raise "Optimization failed" unless $?.success?

    format == 'json' ? JSON.parse(output) : output
  end

  def validate(ship_file, cargo_file)
    cmd = "#{@binary} validate #{ship_file} #{cargo_file} --quiet 2>&1"
    system(cmd)
    $?.success?
  end

  def analyze(results_file)
    cmd = "#{@binary} analyze #{results_file} --quiet 2>&1"
    `#{cmd}`
  end
end

# Usage
cf = CargoForge.new

if cf.validate('ship.cfg', 'cargo.txt')
  result = cf.optimize('ship.cfg', 'cargo.txt')
  puts "Placed: #{result['analysis']['placed_count']}"
  puts "GM: #{result['analysis']['metacentric_height']} m"
end
```

---

## Go Integration

```go
// cargoforge.go
package main

import (
    "encoding/json"
    "os/exec"
)

type CargoForge struct {
    Binary string
}

type Analysis struct {
    PlacedCount       int     `json:"placed_count"`
    TotalCount        int     `json:"total_count"`
    MetacentricHeight float64 `json:"metacentric_height"`
    StabilityStatus   string  `json:"stability_status"`
}

type OptimizationResult struct {
    Analysis Analysis `json:"analysis"`
}

func NewCargoForge() *CargoForge {
    return &CargoForge{Binary: "./cargoforge"}
}

func (cf *CargoForge) Optimize(shipFile, cargoFile string) (*OptimizationResult, error) {
    cmd := exec.Command(
        cf.Binary,
        "optimize",
        shipFile,
        cargoFile,
        "--format=json",
        "--quiet",
    )

    output, err := cmd.Output()
    if err != nil {
        return nil, err
    }

    var result OptimizationResult
    if err := json.Unmarshal(output, &result); err != nil {
        return nil, err
    }

    return &result, nil
}

func (cf *CargoForge) Validate(shipFile, cargoFile string) bool {
    cmd := exec.Command(
        cf.Binary,
        "validate",
        shipFile,
        cargoFile,
        "--quiet",
    )

    err := cmd.Run()
    return err == nil
}

// Usage
func main() {
    cf := NewCargoForge()

    if !cf.Validate("ship.cfg", "cargo.txt") {
        panic("Validation failed")
    }

    result, err := cf.Optimize("ship.cfg", "cargo.txt")
    if err != nil {
        panic(err)
    }

    println("Placed:", result.Analysis.PlacedCount)
    println("GM:", result.Analysis.MetacentricHeight)
}
```

---

## Best Practices

### 1. Error Handling

Always handle errors gracefully:

```python
try:
    result = cf.optimize('ship.cfg', 'cargo.txt')
except subprocess.CalledProcessError as e:
    # Log error
    logger.error(f"Optimization failed: {e.stderr}")
    # Return error response
    return {'error': 'Optimization failed', 'details': str(e)}
```

### 2. Validation First

Always validate inputs before optimization:

```python
if not cf.validate(ship_file, cargo_file):
    raise ValueError("Invalid input files")

result = cf.optimize(ship_file, cargo_file)
```

### 3. Timeout Protection

Prevent long-running processes:

```python
result = subprocess.run(
    cmd,
    capture_output=True,
    timeout=30  # 30 second timeout
)
```

### 4. Temp File Cleanup

Always clean up temporary files:

```python
try:
    result = cf.optimize(ship_file, cargo_file)
finally:
    os.unlink(ship_file)
    os.unlink(cargo_file)
```

### 5. Async for Performance

Use async operations for web APIs:

```python
# FastAPI with async
@app.post('/optimize')
async def optimize(request: Request):
    process = await asyncio.create_subprocess_exec(...)
    stdout, stderr = await process.communicate()
```

### 6. Caching Results

Cache optimization results:

```python
import hashlib
import pickle

def get_cache_key(ship_file, cargo_file):
    with open(ship_file, 'rb') as f1:
        with open(cargo_file, 'rb') as f2:
            content = f1.read() + f2.read()
            return hashlib.md5(content).hexdigest()

def optimize_cached(ship_file, cargo_file):
    key = get_cache_key(ship_file, cargo_file)
    cache_file = f'cache/{key}.pkl'

    if os.path.exists(cache_file):
        with open(cache_file, 'rb') as f:
            return pickle.load(f)

    result = cf.optimize(ship_file, cargo_file)

    with open(cache_file, 'wb') as f:
        pickle.dump(result, f)

    return result
```

---

**For more examples, see [EXAMPLES.md](EXAMPLES.md)**
