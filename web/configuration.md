# Configuration File

Create `~/.cargoforgerc` for global defaults or `.cargoforgerc` in your project directory.

```ini
# Output format (human, json, csv, table, markdown)
format=table

# Colored output (true/false)
color=yes

# Verbose mode
verbose=no

# Show ASCII visualization
show_viz=yes
```

Command-line flags override config file settings. Local config overrides global.

## Supported Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `format` | human, json, csv, table, markdown | human | Default output format |
| `color` | yes, no | auto | Enable colored output |
| `verbose` | yes, no | no | Verbose logging |
| `quiet` | yes, no | no | Quiet mode |
| `show_viz` | yes, no | yes | ASCII visualization |
| `algorithm` | 3d, 2d, auto | 3d | Optimization algorithm |

## Profiles

**API / scripting:**

```ini
format=json
quiet=yes
color=no
show_viz=no
```

**Development / debugging:**

```ini
verbose=yes
color=yes
show_viz=yes
format=table
```

**Report generation:**

```ini
format=markdown
quiet=no
show_viz=no
```

## Quick Setup

```bash
# Global config
echo "format=table" > ~/.cargoforgerc

# Project config (overrides global)
echo "format=json" > .cargoforgerc
```
