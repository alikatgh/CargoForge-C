# CargoForge-C: The Road to Million-Dollar Maritime Software

**Internal Strategy Document — Confidential**

---

## The Core Thesis

Maritime cargo software is a $2.8B market dominated by bloated enterprise Java/C# monoliths (NAPA, Maxsurf, Navis, INTERSCHALT) that cost $50K-$500K per vessel license, take 6-18 months to deploy, and require dedicated IT staff. Every shipping company hates their software but has no alternative.

CargoForge wins by being the opposite: a fast, embeddable C library with zero dependencies that any system can call. Not a replacement for the whole stack — the engine underneath everyone else's stack.

**We don't sell software. We sell the calculation engine that other software can't build themselves.**

---

## Why This Roadmap Is The Only One That Works

### What doesn't work (and why competitors are stuck):

1. **Building another full GUI application** — NAPA and Maxsurf own this. They have 30 years of UI, 200+ employees, and relationships with every class society. You cannot out-feature them. Don't try.

2. **SaaS web platform** — Shipping companies will never put real vessel data on someone else's cloud. Classification societies won't approve it. Insurance won't cover it. This is a regulated industry where a wrong calculation sinks a ship and kills people.

3. **Consulting/services model** — Doesn't scale. You become a body shop. Maritime consultancies already exist (Rightship, ClassNK consulting). Low margins, high churn.

4. **Going after container terminals** — TOS market (Navis N4, Tideworks) is a knife fight. $10M+ sales cycles, 3-year deployments. You'll run out of money before your first deal closes.

### What does work:

**Sell the engine, not the car.**

The maritime industry needs an open, embeddable, auditable stability calculation library that:
- Runs on any hardware (bridge computers run Linux/QNX, not Windows Server)
- Has no runtime dependencies (no JVM, no .NET, no Python)
- Can be validated against class society requirements (open source = auditable)
- Integrates into existing systems via C ABI, JSON API, or WASM

**Every TOS, every ERP, every port management system needs stability calculations. None of them want to build their own. That's the gap.**

---

## Phase 1: Make the Engine Real (Months 1-4)

**Goal: Replace the box-hull model with actual naval architecture that class societies will recognize.**

This is the single most important phase. Without accurate hydrostatics, nothing else matters. A shipping company will test CargoForge against their known stability booklet values within the first hour. If the numbers don't match, they'll never look at it again.

### 1.1 Hydrostatic Table Interpolation

Replace hardcoded coefficients with real hydrostatic data input.

**What to build:**
- Parser for hydrostatic tables (draft vs. KM, KB, BM, TPC, MTC, waterplane area)
- Linear interpolation engine for intermediate drafts
- Support for both even-keel and trimmed hydrostatics (cross-curves of stability)

**Why this specific approach:**
Every ship has a stability booklet from the shipyard. It contains hydrostatic tables. These tables are the ground truth. By interpolating from real tables instead of computing from hull geometry, we:
- Skip the entire CAD/hull modeling problem (that's Maxsurf's job, not ours)
- Get accuracy that matches the ship's approved stability documentation
- Can validate against the same numbers the classification society approved

**File format:** Simple CSV that any naval architect can export from their existing tools.

```
# Hydrostatic table for MV Example
# draft_m,displacement_t,KM_m,KB_m,TPC_t_cm,MTC_t_m,waterplane_m2
1.0,1250,12.50,0.53,8.20,45.0,3200
2.0,2680,9.85,1.06,9.10,52.0,3550
3.0,4200,8.42,1.58,9.85,58.0,3840
...
```

### 1.2 Longitudinal Strength

**What to build:**
- Shear force and bending moment calculations along the hull
- Still water bending moment (SWBM) from weight/buoyancy distribution
- Comparison against permissible limits from class society

**Why this matters:**
A ship can be stable (won't capsize) but still break in half if the weight distribution creates excessive bending. This is the #1 reason cargo plans get rejected by chief officers. The MSC Napoli broke apart in 2007 because of this exact issue.

Every class society (Lloyd's, DNV, ABS) requires SWBM checks. Without this, CargoForge cannot be used for compliance.

### 1.3 Full IMDG Segregation Engine

**What to build:**
- Complete IMDG Code dangerous goods segregation matrix (9 classes, 3 divisions each)
- Stowage category enforcement (on deck only, under deck only, either)
- Segregation types: "away from", "separated from", "separated by a complete compartment from", "separated longitudinally by an intervening complete compartment from"
- EmS (Emergency Schedule) reference lookup

**Why this specific scope:**
The current 3m separation rule is a toy. Real IMDG compliance requires checking a 27x27 compatibility matrix for every pair of dangerous goods. This is also the feature most likely to be mandated by port state control — ports like Rotterdam and Singapore will refuse entry to ships without verified DG segregation plans.

**Data source:** IMDG Code Amendment 41-22 tables. These are public (IMO publishes them). We encode them as a C lookup table — no database needed.

### 1.4 Free Surface Correction

**What to build:**
- Free surface moment calculation for partially filled tanks
- Virtual rise in KG (GG') = sum of (rho * i) / displacement
- Integration into GM calculation

**Why this matters:**
Tankers, bulk carriers with ballast tanks, and any vessel with liquid cargo must account for free surface effects. Ignoring this overestimates GM by up to 0.5m — the difference between "stable" and "capsizes in moderate seas." The MV Sewol disaster in 2014 was directly caused by uncorrected free surface effects.

---

## Phase 2: Make It Embeddable (Months 4-7)

**Goal: Turn CargoForge from a CLI tool into a library that other software calls.**

### 2.1 libcargoforge — Shared Library with C ABI

**What to build:**
- Clean public API header (`cargoforge.h`) with stable ABI
- Shared library build (`.so` / `.dylib` / `.dll`)
- Opaque handle pattern: `CargoForge *cf = cargoforge_create(); cargoforge_load_ship(cf, path); ...`
- Thread-safe design (no global state)
- Error codes and diagnostic messages via callback

**Why C ABI:**
Every language can call C. Python (ctypes/cffi), Java (JNI), C# (P/Invoke), Go (cgo), Rust (FFI), Node.js (ffi-napi). One library, every platform. This is how SQLite became the most deployed database in the world — not by being the best database, but by being the most embeddable one.

### 2.2 JSON-RPC Server Mode

**What to build:**
- `cargoforge serve --port=8080` mode
- JSON-RPC 2.0 over HTTP
- Endpoints: `load_ship`, `add_cargo`, `optimize`, `analyze`, `check_imdg`
- Stateless request/response (ship + cargo in, results out)

**Why this approach:**
Terminal operating systems (TOS) and port management systems often can't link C libraries directly. But every one of them can make HTTP calls. JSON-RPC gives them a clean integration path without us having to write SDKs for every language.

### 2.3 WebAssembly Build

**What to build:**
- Emscripten compilation target
- WASM module with JS bindings
- Same API as the C library but runs in any browser

**Why WASM:**
This turns the web UI from a JavaScript reimplementation into the actual C engine running in the browser. Same code, same results, byte-for-byte identical to the server. Naval architects can validate calculations in their browser before deploying to production.

---

## Phase 3: Get Classification Society Approval (Months 7-12)

**Goal: Get CargoForge's calculation engine approved by at least one major class society.**

This is the phase that turns CargoForge from "interesting open source project" into "software that shipping companies are allowed to use."

### Why This Is Non-Negotiable

In maritime, nothing moves without class society approval. A shipping company cannot use unapproved software for stability calculations — their P&I insurance won't cover losses, port state control will detain the vessel, and the flag state can revoke the ship's certificates.

The class societies that matter:
- **DNV** (Norway) — Largest, most progressive, has a "type approval" program for software
- **Lloyd's Register** (UK) — Oldest, most prestigious
- **Bureau Veritas** (France) — Strong in bulk carriers
- **ClassNK** (Japan) — Dominates Asian fleets
- **ABS** (USA) — Strong in offshore and US-flag vessels

### 3.1 DNV Type Approval (Primary Target)

**What to do:**
- Apply for DNV Type Approval under their "Computer Software" program (DNV-SE-0475)
- Prepare test documentation: calculation methods, validation against benchmark vessels, test coverage
- Submit source code for review (this is where open source is an advantage — nothing to hide)

**What DNV requires:**
1. Mathematical description of all calculation methods
2. Validation against at least 3 benchmark vessels with known stability booklets
3. Comparison against approved software (they'll compare against NAPA)
4. Quality management documentation
5. Regression test suite with >95% code coverage

**Cost:** ~$30K-$60K for the approval process
**Timeline:** 3-6 months after submission
**Value:** Once approved, every DNV-classed vessel (>13,000 ships) can use CargoForge

### 3.2 Benchmark Vessel Validation

**What to build:**
- Test harness that loads a vessel's stability booklet data and compares CargoForge results
- At minimum 3 vessel types: container ship, bulk carrier, general cargo
- Tolerance thresholds per class society requirements (typically <1% on displacement, <0.01m on GM)

**Where to get benchmark data:**
- Published IMO model test cases (freely available)
- University of Strathclyde naval architecture department (publish research vessels)
- Contact ship operators who want to be early adopters — they'll provide their booklet data for free testing

### 3.3 Validation Documentation

**What to produce:**
- Technical manual describing every calculation method with equations and references
- Test report showing validation results against benchmark vessels
- User manual (the USAGE.md we already have, expanded)
- Change management procedure (how we track and validate code changes)

---

## Phase 4: First Revenue (Months 10-18)

### 4.1 The Business Model

**Open core:**
- C engine (libcargoforge) — MIT licensed, free forever
- CLI tool — MIT licensed, free forever
- Web UI — MIT licensed, free forever

**What we charge for:**
- **Hydrostatic table library** — Curated, validated tables for 500+ vessel types. $2,000/year per vessel. Updated quarterly.
- **IMDG Code database updates** — Biennial IMDG amendments encoded and tested. $5,000/year.
- **Class society validation reports** — Pre-computed validation for specific vessel/class combinations. $1,000 per vessel.
- **Integration support** — Help TOS/ERP vendors embed libcargoforge. $50K-$200K per integration.
- **Priority support** — SLA-backed support for production deployments. $10K-$50K/year.

### 4.2 First Customers (In Order)

**Target 1: Ship management companies (months 10-12)**

Companies like V.Ships, Anglo-Eastern, Bernhard Schulte manage 500-2,000 vessels each. They employ fleet superintendents who currently use Excel spreadsheets and the vessel's approved loading computer. CargoForge gives them a fleet-wide calculation engine they can script and automate.

- Entry point: Free CLI tool for the superintendent's laptop
- Upsell: Hydrostatic table library for their fleet ($2K/vessel x 500 vessels = $1M/year)
- Decision maker: Fleet Technical Director

**Target 2: Port terminal operators (months 12-15)**

Container terminals (PSA, Hutchison, APM, DP World) need to verify vessel stability before allowing cargo operations. Currently they trust the ship's crew or use expensive on-site consultants.

- Entry point: JSON-RPC server integrated into their TOS
- Upsell: Integration support ($100K+) plus annual license
- Decision maker: Terminal Operations Director

**Target 3: TOS/ERP software vendors (months 15-18)**

Navis, Tideworks, Cargotec, WiseTech — these companies build the software that terminals use. They all need stability calculations and none of them want to build their own.

- Entry point: libcargoforge as embedded engine
- Upsell: OEM license + integration support ($200K+ per vendor)
- Decision maker: VP Engineering / CTO

### 4.3 Revenue Projections (Conservative)

| Year | Source | Revenue |
|------|--------|---------|
| 1 | 3 ship managers (avg 200 vessels), integration support | $600K |
| 2 | 10 ship managers, 2 terminal operators, 1 OEM vendor | $2.5M |
| 3 | 25 ship managers, 5 terminals, 3 OEM vendors, IMDG subscriptions | $6M |

**The hockey stick:** Once 1 TOS vendor embeds CargoForge, every terminal using that TOS becomes a user. Navis N4 alone is deployed at 300+ terminals worldwide.

---

## Phase 5: Become the Standard (Months 18-36)

### 5.1 IMO Recognition

Submit CargoForge's calculation methods to IMO's Sub-Committee on Ship Design and Construction (SDC). Goal: get referenced in MSC circulars as an accepted calculation method.

This is not about ego. IMO reference means flag states accept the software, which means every vessel flying that flag can use it. Panama alone has 8,600+ vessels.

### 5.2 Open Standard for Vessel Data Exchange

Publish a specification for vessel hydrostatic data interchange. Currently every software vendor uses their own proprietary format. We define the open one. If everyone uses our format, everyone needs our parser, which ships with libcargoforge.

This is the SQLite playbook: become so ubiquitous and so easy to embed that replacing you costs more than keeping you.

### 5.3 Regulatory Compliance Automation

Build automated compliance checking for:
- SOLAS Chapter II-1 (stability requirements)
- MARPOL Annex I (oil tanker stability)
- International Grain Code (grain cargo stability)
- CSS Code (cargo securing)

Each regulation is a new revenue stream. Ship managers will pay for automated compliance because the alternative is hiring a naval architect for $150K/year.

---

## Why Open Source Is The Weapon, Not The Weakness

Every competitor in maritime software is closed source. This is their vulnerability.

1. **Auditability** — Class societies can read our code. They can verify our calculations line by line. Closed-source competitors submit black-box test results. When DNV reviewers can `git blame` the GZ curve implementation, trust goes up. Approval timeline goes down.

2. **Adoption speed** — A naval architect in Manila can download CargoForge tonight and validate it against their vessel's stability booklet by morning. No sales call, no demo, no procurement. By the time the purchasing department gets involved, the technical decision is already made.

3. **Integration cost** — An OEM vendor evaluating libcargoforge can prototype the integration in a week. Evaluating a closed-source competitor means NDAs, license negotiations, and 3 months of back-and-forth. We win on time-to-integration.

4. **Community contributions** — Naval architects at universities, independent surveyors, and class society researchers will improve the code for free because they need it for their own work. NAPA charges universities $25K/year. We charge $0.

5. **Lock-in protection** — Shipping companies have been burned by vendor lock-in (ask anyone who migrated from GL ShipLoad to NAPA). Open source means they can always fork. This makes them more willing to adopt, not less.

The revenue comes from the ecosystem around the engine, not the engine itself. This is the Red Hat model, the Elastic model, the Hashicorp model. The code is free. The data, the validation, the support, and the integration — that's where the money is.

---

## What We Build Next (Priority Order)

| Priority | Feature | Why It's Next | Revenue Impact |
|----------|---------|---------------|----------------|
| 1 | Hydrostatic table interpolation | Without this, nothing else matters | Unlocks class approval |
| 2 | Longitudinal strength (SWBM) | Required for class approval | Unlocks class approval |
| 3 | Full IMDG segregation matrix | #1 requested feature in maritime | $5K/year subscription |
| 4 | Free surface correction | Required for tanker/bulker use | Unlocks 60% of world fleet |
| 5 | libcargoforge shared library | Enables embedding/OEM | Unlocks OEM revenue |
| 6 | JSON-RPC server mode | Enables TOS integration | Unlocks terminal revenue |
| 7 | DNV type approval submission | Regulatory gate | Unlocks all revenue |
| 8 | Hydrostatic table library (data) | First subscription product | $2K/vessel/year |
| 9 | WASM build | Browser-based validation | Reduces sales friction |
| 10 | Grain stability (Int'l Grain Code) | Bulk carrier market | New market segment |

---

## The Bottom Line

CargoForge doesn't compete with NAPA. CargoForge is the SQLite to NAPA's Oracle.

NAPA is a $500K enterprise deployment. CargoForge is a 200KB library that runs on the bridge computer. NAPA requires a naval architecture department. CargoForge requires `#include "cargoforge.h"`.

The maritime industry is moving toward automation (MASS — Maritime Autonomous Surface Ships), digital twins, and real-time stability monitoring. All of these need an embeddable, fast, validated calculation engine. That's what we're building.

The market is $2.8B. We need 0.04% of it to hit $1M ARR.

Build the engine. Get it approved. Let the market come to us.
