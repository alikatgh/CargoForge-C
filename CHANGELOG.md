# Changelog

All notable changes to CargoForge-C will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
