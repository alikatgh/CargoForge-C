# CargoForge-C

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/github/actions/workflow/status/alikatgh/CargoForge-C/ci.yml)](https://github.com/alikatgh/CargoForge-C/actions)
[![Contributors](https://img.shields.io/github/contributors/alikatgh/CargoForge-C.svg)](https://github.com/alikatgh/CargoForge-C/graphs/contributors)
[![GitHub issues](https://img.shields.io/github/issues/alikatgh/CargoForge-C.svg)](https://github.com/alikatgh/CargoForge-C/issues)

> **Professional Maritime Cargo Loading Optimization Platform**
> Production-ready SaaS application with subscription billing, PDF exports, and API access.

**CargoForge-C** is a commercial-grade maritime cargo optimization platform combining a high-performance C99 computation engine with a modern web interface, user authentication, and payment processing. Perfect for maritime schools, shipping companies, logistics firms, and educational institutions.

![CargoForge Demo](DEMO.md)

-----

## 🌟 Features

### Core Optimization Engine (C99)
- ✅ **3D Guillotine Bin-Packing**: Advanced space-filling algorithm with 6-orientation testing
- ✅ **Naval Architecture Calculations**: Block coefficient (Cb), waterplane coefficient (Cw), KB/BM/GM analysis
- ✅ **Cargo Constraints**: Hazmat separation (3m minimum), point load limits, weight distribution rules
- ✅ **Stability Analysis**: IMO-compliant metacentric height calculations with status classification
- ✅ **Zero Dependencies**: Pure C99 with no external libraries for maximum performance
- ✅ **JSON API**: Modern JSON output for seamless web integration

### Web Platform (Production SaaS)
- 🚀 **Interactive 3D Visualization**: Hardware-accelerated Three.js rendering with orbit controls
- 🎯 **Training Mode**: Progressive challenges from beginner to advanced
- 👤 **User Authentication**: Secure registration, login, and session management
- 💳 **Subscription Billing**: Stripe integration with Free, Pro, and Enterprise tiers
- 📄 **PDF Export**: Professional multi-page loading plan reports
- 📊 **Usage Analytics**: Real-time dashboard with statistics and insights
- 🔑 **REST API**: Full API access with rate-limited endpoints
- 💾 **Scenario Management**: Save, share, and collaborate on cargo plans

### Enterprise Features
- 🏢 **Multi-User Support**: Team collaboration with role-based access
- 🔒 **Security**: Bcrypt password hashing, API key authentication, CORS protection
- ⚡ **Rate Limiting**: Redis-backed limits per subscription tier
- 📈 **Monitoring**: Sentry error tracking and application logs
- 🐳 **Docker Deployment**: Production-ready containerization
- 📚 **Comprehensive Documentation**: API docs, deployment guide, and user manual

-----

## 🚀 Quick Start

### Try the Demo (Fastest)

```bash
# Clone and build
git clone https://github.com/alikatgh/CargoForge-C.git
cd CargoForge-C
make

# Start web interface (demo mode)
cd web && ./START.sh

# Open http://localhost:5000
```

### Production Deployment

```bash
# Copy and configure environment
cp .env.example .env
nano .env  # Add your Stripe keys, database URL, etc.

# Deploy with Docker
chmod +x deploy.sh
./deploy.sh

# Access at https://yourdomain.com
```

See [DEPLOYMENT.md](DEPLOYMENT.md) for complete production setup guide.

-----

## 🖥️ CLI Usage

CargoForge-C v2.0 features a powerful command-line interface with subcommands, multiple output formats, and advanced options.

### Basic Commands

```bash
# Display help
./cargoforge --help
./cargoforge --version

# Run optimization
./cargoforge optimize ship.cfg cargo.txt

# Validate inputs before optimization
./cargoforge validate ship.cfg cargo.txt

# Display ship information
./cargoforge info ship.cfg

# Interactive mode (guided wizard)
./cargoforge interactive
```

### Output Formats

CargoForge-C supports multiple output formats for different use cases:

```bash
# Human-readable output (default)
./cargoforge optimize ship.cfg cargo.txt

# JSON output for API integration
./cargoforge optimize ship.cfg cargo.txt --format=json

# CSV export for spreadsheets
./cargoforge optimize ship.cfg cargo.txt --format=csv > results.csv

# ASCII table format
./cargoforge optimize ship.cfg cargo.txt --format=table

# Markdown report
./cargoforge optimize ship.cfg cargo.txt --format=markdown > report.md
```

### Advanced Options

```bash
# Verbose output with detailed logging
./cargoforge optimize ship.cfg cargo.txt --verbose

# Quiet mode (suppress non-essential output)
./cargoforge optimize ship.cfg cargo.txt --quiet

# Disable colored output
./cargoforge optimize ship.cfg cargo.txt --no-color

# Disable ASCII visualization
./cargoforge optimize ship.cfg cargo.txt --no-viz

# Save output to file
./cargoforge optimize ship.cfg cargo.txt --output=results.json --format=json
```

### Filtering Options

```bash
# Show only successfully placed cargo
./cargoforge optimize ship.cfg cargo.txt --only-placed

# Show only failed cargo items
./cargoforge optimize ship.cfg cargo.txt --only-failed

# Filter by cargo type
./cargoforge optimize ship.cfg cargo.txt --type=hazardous
```

### Subcommand-Specific Help

```bash
# Get help for a specific subcommand
./cargoforge optimize --help
./cargoforge validate --help
./cargoforge info --help
```

### Example Workflows

```bash
# 1. Validate inputs before running optimization
./cargoforge validate ship.cfg cargo.txt && \
./cargoforge optimize ship.cfg cargo.txt

# 2. Generate multiple output formats
./cargoforge optimize ship.cfg cargo.txt --format=json > results.json
./cargoforge optimize ship.cfg cargo.txt --format=markdown > report.md
./cargoforge optimize ship.cfg cargo.txt --format=csv > data.csv

# 3. Get detailed ship information
./cargoforge info examples/sample_ship.cfg examples/sample_cargo.txt

# 4. Verbose validation for debugging
./cargoforge validate ship.cfg cargo.txt --verbose

# 5. API-style usage with JSON
./cargoforge optimize ship.cfg cargo.txt --format=json --quiet > output.json
```

### Input File Formats

**Ship Configuration (.cfg)**
```
length_m=150
width_m=25
max_weight_tonnes=50000
lightship_weight_tonnes=2000
lightship_kg_m=8.0
```

**Cargo Manifest (.txt)**
```
# ID              Weight(t) Dimensions(LxWxH)    Type
HeavyMachinery    550       20x5x3               standard
ContainerA        250       12.2x2.4x2.6         reefer
HazmatCargo       100       10x3x3               hazardous
FragileGoods      50        5x5x5                fragile
```

**Cargo Types:**
- `standard` - Regular cargo
- `hazardous` - Hazmat (requires 3m separation)
- `reefer` - Refrigerated cargo
- `fragile` - Fragile items (stacking restrictions)
- `bulk` - Bulk cargo
- `general` - General goods

### Backward Compatibility

The new CLI maintains backward compatibility with the legacy interface:

```bash
# Legacy format (still supported)
./cargoforge ship.cfg cargo.txt --json
./cargoforge ship.cfg cargo.txt --no-viz

# Equivalent new format
./cargoforge optimize ship.cfg cargo.txt --format=json
./cargoforge optimize ship.cfg cargo.txt --no-viz
```

-----

## 💰 Pricing & Subscription Tiers

| Feature | Free | Pro | Enterprise |
|---------|------|-----|------------|
| **Daily Optimizations** | 10 | 1,000 | Unlimited |
| **Save Scenarios** | ❌ | ✅ Unlimited | ✅ Unlimited |
| **PDF Export** | ❌ | ✅ | ✅ |
| **API Access** | ❌ | ✅ | ✅ |
| **3D Visualization** | ✅ | ✅ | ✅ |
| **Training Challenges** | ✅ | ✅ | ✅ |
| **Priority Support** | ❌ | ✅ | ✅ Dedicated |
| **Custom Features** | ❌ | ❌ | ✅ |
| **SLA Guarantee** | ❌ | ❌ | ✅ 99.9% |
| **Pricing** | **$0** | **$29/month** | **Custom** |

-----

## 🎓 Use Cases

### Maritime Training Schools
- Interactive learning platform for naval architecture students
- Progressive challenges teaching stability and loading principles
- Visual feedback with 3D rendering
- Export professional reports for assignments

### Shipping Companies
- Optimize cargo loading for fuel efficiency
- Ensure IMO stability compliance
- Generate loading plans for crew
- API integration with logistics software

### Logistics & Freight Forwarders
- Container loading optimization
- Multi-cargo scenario planning
- Client reporting with PDF exports
- Team collaboration on complex loads

### Research & Education
- Algorithm benchmarking and validation
- Maritime safety research
- Open-source C99 codebase for study
- REST API for data collection

-----

## 📖 Documentation

| Document | Description |
|----------|-------------|
| [DEPLOYMENT.md](DEPLOYMENT.md) | Complete production deployment guide |
| [API.md](API.md) | REST API documentation with examples |
| [DEMO.md](DEMO.md) | Interactive demo walkthrough |
| [CHANGELOG.md](CHANGELOG.md) | Version history and release notes |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution guidelines |

-----

## 🛠 Technical Stack

### Backend
- **Computation Engine**: Pure C99 (GCC/Clang)
- **Web Framework**: Flask 3.0 (Python)
- **Database**: PostgreSQL 15 with SQLAlchemy ORM
- **Authentication**: Flask-Login + Bcrypt
- **Payments**: Stripe API
- **PDF Generation**: ReportLab
- **Rate Limiting**: Flask-Limiter + Redis
- **Server**: Gunicorn WSGI

### Frontend
- **Rendering**: Three.js for 3D visualization
- **UI**: Vanilla JavaScript with modern CSS Grid
- **Styles**: Gradient design system
- **Responsive**: Mobile-first design

### DevOps
- **Containerization**: Docker + Docker Compose
- **Reverse Proxy**: Nginx with SSL/TLS
- **Monitoring**: Sentry for error tracking
- **CI/CD**: GitHub Actions (planned)
- **Backup**: Automated PostgreSQL backups

-----

## 📊 Architecture

```
┌─────────────┐
│   Browser   │
└──────┬──────┘
       │ HTTPS
       ↓
┌─────────────┐      ┌──────────────┐
│    Nginx    │─────→│  Flask API   │
│  (Port 80)  │      │  (Port 5000) │
└─────────────┘      └───────┬──────┘
                             │
                    ┌────────┼────────┐
                    ↓        ↓        ↓
              ┌──────────┐ ┌────┐ ┌──────────┐
              │ C Binary │ │ DB │ │  Redis   │
              │ (JSON)   │ │    │ │ (Limits) │
              └──────────┘ └────┘ └──────────┘
                             │
                             ↓
                      ┌─────────────┐
                      │   Stripe    │
                      │  (Payments) │
                      └─────────────┘
```

-----

## 🔧 Installation

### Prerequisites

**Required:**
- C Compiler (GCC 9+ or Clang 10+)
- Python 3.8+
- PostgreSQL 13+ (or SQLite for dev)
- Redis 6+ (for rate limiting)
- Make or CMake

**For Production:**
- Docker & Docker Compose
- Domain with SSL certificate
- Stripe account
- Email service (SMTP)

### Development Setup

```bash
# 1. Clone repository
git clone https://github.com/alikatgh/CargoForge-C.git
cd CargoForge-C

# 2. Build C binary
make clean && make
make test  # Run tests

# 3. Setup Python environment
cd web/backend
pip install -r requirements-prod.txt

# 4. Configure environment
cp ../../.env.example ../../.env
# Edit .env with your settings

# 5. Run development server
python app_prod.py

# Access at http://localhost:5000
```

### Production Deployment

See [DEPLOYMENT.md](DEPLOYMENT.md) for:
- Docker deployment
- SSL/HTTPS setup
- Stripe configuration
- Database migration
- Nginx configuration
- Monitoring setup
- Backup procedures

-----

## 🔌 API Usage

### Authentication

```bash
# Register user
curl -X POST https://api.cargoforge.com/api/auth/register \
  -H "Content-Type: application/json" \
  -d '{"email":"user@example.com","password":"secure123"}'

# Get API key from response
export API_KEY="your_api_key_here"
```

### Run Optimization

```bash
curl -X POST https://api.cargoforge.com/api/optimize \
  -H "X-API-Key: $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "ship": {
      "length": 150,
      "width": 25,
      "depth": 12,
      "max_draft": 10,
      "displacement": 50000
    },
    "cargo": [
      {
        "id": "Container1",
        "weight": 25000,
        "dimensions": [12, 2.4, 2.6],
        "type": "standard"
      }
    ]
  }'
```

### Export PDF

```bash
curl -X POST https://api.cargoforge.com/api/export/pdf \
  -H "X-API-Key: $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"ship":{...},"cargo":[...]}' \
  --output cargo_plan.pdf
```

See [API.md](API.md) for complete API documentation with examples in Python, JavaScript, PHP, and cURL.

-----

## 🧪 Testing

```bash
# Unit tests (C)
make test

# Memory safety checks
make test-asan      # AddressSanitizer + UBSan
make test-valgrind  # Valgrind leak checker

# Python tests (planned)
cd web/backend
pytest tests/

# Integration tests (planned)
./scripts/integration_test.sh
```

-----

## 📂 Project Structure

```
CargoForge-C/
├── Core C Engine (CLI)
│   ├── main.c                    # Entry point
│   ├── cli.c/h                   # CLI parser & subcommands (NEW v2.0)
│   ├── parser.c/h                # Input parsing
│   ├── optimizer.c/h             # Optimization coordinator
│   ├── placement_3d.c/h          # 3D bin-packing (guillotine)
│   ├── placement_2d.c/h          # Legacy 2D placement
│   ├── constraints.c/h           # Cargo constraint validation
│   ├── analysis.c/h              # Stability calculations
│   ├── visualization.c/h         # ASCII output
│   ├── json_output.c/h           # JSON serialization
│   └── cargoforge.h              # Main header file
├── web/
│   ├── backend/
│   │   ├── app_prod.py           # Production Flask app
│   │   ├── pdf_export.py         # PDF generation
│   │   ├── stripe_integration.py # Payment processing
│   │   └── requirements-prod.txt # Python dependencies
│   └── frontend/
│       ├── landing.html          # Marketing page
│       ├── index.html            # Simulator app
│       ├── dashboard.html        # User dashboard
│       ├── terms.html            # Terms of Service
│       └── privacy.html          # Privacy Policy
├── tests/                        # C unit tests
├── examples/                     # Sample input files
├── docs/                         # Generated documentation
├── docker-compose.yml            # Multi-service orchestration
├── Dockerfile                    # Container image
├── deploy.sh                     # Deployment script
├── Makefile                      # Make build
├── CMakeLists.txt                # CMake build
├── .env.example                  # Environment template
├── DEPLOYMENT.md                 # Deployment guide
├── API.md                        # API documentation
├── DEMO.md                       # Demo guide
└── README.md                     # This file
```

-----

## 🎯 Roadmap

### Completed ✅
- [x] **v0.1-alpha**: Core 2D simulator, file parsing, basic stability
- [x] **v0.2-beta**: 3D bin-packing, constraints, comprehensive tests
- [x] **v0.2-web**: Interactive web UI with Three.js visualization
- [x] **v0.3-production**: User auth, subscriptions, PDF export, Stripe billing
- [x] **v2.0**: Modern CLI with subcommands, multiple output formats, interactive mode

### In Progress 🚧
- [ ] **v2.1**: Email notifications, advanced analytics dashboard
- [ ] **v0.5**: Team collaboration features, role-based access control
- [ ] **v0.6**: Mobile apps (iOS/Android) with offline mode

### Planned 📋
- [ ] **v0.7**: Machine learning optimization (neural networks)
- [ ] **v0.8**: Fleet-wide optimization (multiple ships)
- [ ] **v0.9**: Real-time sensor integration (IoT devices)
- [ ] **v1.0**: Stable API, enterprise SLA, white-label solution

-----

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Areas for contribution:**
- 🧮 Advanced optimization algorithms (genetic, simulated annealing)
- 🎨 UI/UX improvements
- 📊 Data visualization enhancements
- 🧪 Test coverage expansion
- 📝 Documentation improvements
- 🌍 Internationalization (i18n)
- 🐛 Bug fixes

-----

## 📜 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

**Commercial Use:** Fully permitted under MIT license. For enterprise licensing, custom features, or support contracts, contact: enterprise@cargoforge.com

-----

## 🙏 Acknowledgments

- **Naval Architecture**: Formulas based on IMO stability guidelines
- **Algorithms**: Guillotine bin-packing research from computational geometry
- **Inspiration**: Real-world maritime logistics challenges
- **Community**: Contributors and testers who helped shape this project

-----

## 📞 Support & Contact

- 📧 **Email**: support@cargoforge.com
- 💬 **Issues**: [GitHub Issues](https://github.com/alikatgh/CargoForge-C/issues)
- 📚 **Documentation**: [docs.cargoforge.com](https://docs.cargoforge.com)
- 🐦 **Twitter**: [@CargoForge](https://twitter.com/CargoForge)
- 💼 **Enterprise**: enterprise@cargoforge.com

-----

## ⭐ Star History

If you find this project useful, please consider giving it a star! It helps others discover the project.

[![Star History](https://api.star-history.com/svg?repos=alikatgh/CargoForge-C&type=Date)](https://star-history.com/#alikatgh/CargoForge-C&Date)

-----

**Built with ❤️ for the maritime industry**
