# Examples

## Basic Optimization

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

## Full Analysis

```bash
# Hydrostatic tables + tanks + strength limits
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo.txt
```

## Dangerous Goods

```bash
./cargoforge optimize examples/sample_ship_full.cfg examples/sample_cargo_dg.txt
```

## Validate First

```bash
./cargoforge validate ship.cfg cargo.txt && \
  ./cargoforge optimize ship.cfg cargo.txt
```

## Output Pipelines

```bash
# JSON to jq
./cargoforge optimize ship.cfg cargo.txt --format=json -q | jq '.analysis'

# Save and analyze
./cargoforge optimize ship.cfg cargo.txt --format=json --output=results.json
./cargoforge analyze results.json

# Optimize + analyze in one pipeline
./cargoforge optimize ship.cfg cargo.txt --format=json | ./cargoforge analyze -

# Markdown report
./cargoforge optimize ship.cfg cargo.txt --format=markdown --output=report.md
```

## Filtering

```bash
# Only failed placements
./cargoforge optimize ship.cfg cargo.txt --format=table --only-failed

# By cargo type
./cargoforge optimize ship.cfg cargo.txt --format=table --type=hazardous
```

## Batch Processing

```bash
for cargo in cargos/*.txt; do
  ./cargoforge optimize ship.cfg "$cargo" --quiet \
    --format=json --output="results/$(basename $cargo .txt).json"
done
```

## Python Integration

```python
import subprocess, json

result = subprocess.run(
    ['./cargoforge', 'optimize', 'ship.cfg', 'cargo.txt', '--format=json', '--quiet'],
    capture_output=True, text=True, check=True
)
data = json.loads(result.stdout)
print(f"Placed: {data['analysis']['placed_count']}")
print(f"GM: {data['analysis']['metacentric_height']:.2f} m")
```

## Bash Script

```bash
#!/bin/bash
if cargoforge validate ship.cfg cargo.txt --quiet; then
  cargoforge optimize ship.cfg cargo.txt --format=json --output=results.json
else
  echo "Validation failed"
  exit 1
fi
```

## Docker

```dockerfile
FROM gcc:12
WORKDIR /app
COPY . .
RUN make clean && make
ENTRYPOINT ["./cargoforge"]
CMD ["--help"]
```

```bash
docker build -t cargoforge .
docker run -v $(pwd)/data:/data cargoforge optimize /data/ship.cfg /data/cargo.txt --format=json
```
