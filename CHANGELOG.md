# Changelog

All notable changes to CargoForge-C will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.1.0] - 2025-11-17

### Added - Power User Features 🔥
- **Stdin Support** - Read ship or cargo data from pipes using `-` as filename
  - `cat cargo.txt | ./cargoforge optimize ship.cfg -`
  - Chain commands: `./cargoforge optimize ... --format=json | ./cargoforge analyze -`
  - Works with all file input parameters
- **Analyze Subcommand** - Full-featured JSON results analyzer
  - Parses saved JSON optimization results
  - Displays detailed statistics and recommendations
  - Shows ship specs, cargo summary, weight analysis, stability metrics
  - Provides actionable warnings and suggestions
- **Configuration File Support** - Set default CLI options
  - Global config: `~/.cargoforgerc`
  - Project config: `./.cargoforgerc` (overrides global)
  - Supported options: format, color, verbose, quiet, show_viz, algorithm
  - Command-line flags override config settings
- **Enhanced JSON Output** - Fixed `--output` flag for JSON format
  - Now correctly writes JSON to specified file

### Changed
- Modified `parser.c` to support stdin with `-` filename
- Updated `cli.c` with config file parsing and enhanced analyze command
- Fixed `output_results()` to handle JSON file output

### Examples
```bash
# Stdin support
cat cargo.txt | ./cargoforge optimize ship.cfg -

# Analyze command
./cargoforge optimize ship.cfg cargo.txt --output=results.json --format=json
./cargoforge analyze results.json

# Config file
echo "format=table" > ~/.cargoforgerc
./cargoforge optimize ship.cfg cargo.txt  # Uses table by default
```

## [2.0.0] - 2025-11-17

### Added - CLI Modernization 🚀
- **Modern CLI architecture** with subcommand system (optimize, validate, info, interactive, version, help)
- **getopt_long argument parser** for professional command-line interface
- **Multiple output formats**: JSON, CSV, Table, Markdown, and Human-readable
- **Interactive mode** - Step-by-step wizard for creating ship and cargo configuration files
- **Validate subcommand** - Input validation without running optimization
- **Info subcommand** - Display ship and cargo statistics
- **Comprehensive help system** - Global help and subcommand-specific help (--help flag)
- **Version information** - Build date and version display (--version flag)
- **Advanced filtering** - Filter results by placement status (--only-placed, --only-failed) or cargo type
- **Output control** - Save results to file with --output flag
- **Colored terminal output** - Auto-detected with --no-color override
- **Verbose/Quiet modes** - Control output verbosity with -v/--verbose and -q/--quiet
- **Better error messages** - Contextual errors with file names and line numbers
- **Progress indicators** - Visual feedback for long operations
- **Backward compatibility** - Legacy --json and --no-viz flags still supported

### Changed
- Refactored main.c to use new CLI architecture (simplified to ~50 lines)
- Added cli.c (1000+ lines) with all CLI logic and subcommand handlers
- Added cli.h with CLI structures, enums, and function prototypes
- Updated Makefile to include cli.c and cli.h in build
- Enhanced README with comprehensive CLI documentation and usage examples
- Updated project roadmap to include v2.0 milestone

### Technical Details
- New files: cli.c, cli.h
- Exit codes for different error types (invalid args, file errors, parse errors, etc.)
- CLIContext struct for managing all command-line options
- Subcommand dispatch system with handler functions
- OutputFormat enum supporting 5 different formats
- ANSI color codes with terminal detection
- Support for --help on any subcommand

### Examples
```bash
# Modern subcommand interface
./cargoforge optimize ship.cfg cargo.txt --format=json
./cargoforge validate ship.cfg cargo.txt --verbose
./cargoforge info ship.cfg
./cargoforge interactive

# Multiple output formats
./cargoforge optimize ship.cfg cargo.txt --format=csv > results.csv
./cargoforge optimize ship.cfg cargo.txt --format=markdown > report.md
./cargoforge optimize ship.cfg cargo.txt --format=table

# Advanced filtering
./cargoforge optimize ship.cfg cargo.txt --only-failed
./cargoforge optimize ship.cfg cargo.txt --type=hazardous
```

## [0.3.0-production] - Previous Release

### Added
- **3D bin-packing algorithm** using guillotine heuristic for realistic cargo placement
- **Cargo constraints system** for hazmat separation, fragile cargo, and weight distribution
- **Improved stability calculations** with realistic naval architecture coefficients
- **Comprehensive test suite** including tests for analysis module (7 test cases)
- **CMake build system** with cross-platform support and sanitizer options
- **Doxygen configuration** for API documentation generation
- **Memory safety testing** with ASAN, UBSAN, and Valgrind support
- LICENSE file (MIT)
- Comprehensive .gitignore

### Changed
- Upgraded from 2D shelf-based to true 3D bin-packing with 6 orientations per cargo
- Improved stability analysis using block coefficient, waterplane coefficient, and proper KB/BM/GM calculations
- Updated README to accurately reflect implemented vs planned features
- Refactored optimizer to use new 3D placement module
- Enhanced Makefile with test-asan and test-valgrind targets

### Fixed
- Removed development artifacts (.bak files)
- Cleaned up dev comments from source code
- Fixed GM calculation to use realistic maritime coefficients
- Corrected stability assessment thresholds

## [0.1.0-alpha] - 2025-01-XX

### Added
- Initial 2D cargo placement using First-Fit Decreasing algorithm
- Basic stability calculations (center of gravity, metacentric height)
- Ship configuration and cargo manifest parsing
- Sample ship and cargo files
- Basic test suite for parser and placement modules
- GitHub Actions CI/CD pipeline
- CONTRIBUTING.md and Code of Conduct

### Known Limitations
- Only 2D placement (height dimension not fully utilized)
- Simplified stability model (box-hull approximation)
- Hardcoded bin definitions
- No cargo type-specific handling
- GM values unrealistically high for lightly loaded ships
