# Continuous integration

Every time a developer pushes code to CargoForge-C, a server on the internet automatically compiles the project, runs every test, and reports whether anything broke — before a single human reviewer reads a line. That automated pipeline is **continuous integration** (CI). This lesson explains what CI is, why it matters, and how CargoForge-C's workflow file encodes the exact gates every push must pass.

---

## What CI actually does

Without CI, bugs accumulate between reviews. A developer fixes stability calculations, a colleague adds a parsing feature, and neither notices the two changes interact until a user reports a crash three weeks later. CI short-circuits that delay: the moment code is pushed, the pipeline runs on a clean machine that has never seen the developer's local environment. If the code breaks on that neutral machine, the pipeline fails and the branch is marked red before anyone merges it.

The word "continuous" refers to the frequency: not nightly, not weekly — every push, every pull request.

The word "integration" refers to what is being tested: not a single function in isolation, but the whole project compiled and exercised together, the way a real user would experience it.

---

## The workflow file

CI for CargoForge-C is configured in `.github/workflows/c-build.yml`. GitHub reads this file and executes it on GitHub Actions infrastructure whenever the trigger conditions are met. Here is the complete file:

```yaml
# .github/workflows/c-build.yml

name: C/C++ CI

# This workflow runs on pushes and pull requests to the main branch
on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]

jobs:
  build:
    # Use the latest version of Ubuntu to run the job
    runs-on: ubuntu-latest

    steps:
      # Step 1: Check out your repository code
      - name: Check out code
        uses: actions/checkout@v4

      # Step 2: Compile the project using your Makefile
      - name: Compile the project
        run: make

      # Step 3: Run a basic test to ensure the executable works
      - name: Run a basic test
        run: ./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
```

Every concept in this file has a specific job. Read it section by section.

---

## Section: `on`

```yaml
on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]
```

This is the **trigger**. GitHub will run the pipeline when:

- Someone pushes commits directly to `main`, or
- Someone opens or updates a pull request that targets `main`.

The `branches` filter keeps the pipeline from running on every experimental branch a developer might create. Only code that is heading toward `main` — the canonical, deployable version — must pass the gates.

!!! note
    Pull request triggers are the most important. A pull request is a proposal to merge; the CI result appears as a green check or red cross on the PR page, giving reviewers an objective signal before they read any code.

---

## Section: `jobs` and `runs-on`

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
```

A **job** is an isolated unit of work that runs on a virtual machine. `ubuntu-latest` tells GitHub to provision a fresh Ubuntu Linux VM. This machine has no history: no prior builds, no cached object files, no developer-specific environment variables. If the code cannot build there, it will not build for a new contributor either.

CargoForge-C targets C99 with POSIX extensions (`-D_POSIX_C_SOURCE=200809L`). Ubuntu ships with GCC and `make` pre-installed, so no setup step is needed beyond checking out the code.

---

## Section: `steps`

Steps run sequentially within the job. If any step exits with a non-zero status code, subsequent steps are skipped and the entire job is marked **failed**.

### Step 1 — Check out code

```yaml
- name: Check out code
  uses: actions/checkout@v4
```

`actions/checkout@v4` is a pre-built action maintained by GitHub. It clones the repository at the exact commit being tested — for a push, the new commit; for a pull request, the merge commit that would result if the PR were accepted. Without this step the VM has no source code at all.

### Step 2 — Compile

```yaml
- name: Compile the project
  run: make
```

`make` invokes the Makefile's default target, `all`, which compiles every source file in `src/` into object files under `build/` and links them into the `cargoforge` binary. The compile flags are fixed in the Makefile:

```
-O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude
```

`-Wall -Wextra` mean the compiler will produce warnings for common mistakes (unused variables, implicit function declarations, signed/unsigned mismatches). In CargoForge-C those warnings are treated as informational — the CI step fails only if the compiler exits non-zero, which happens on errors, not warnings. A stricter setting would add `-Werror` to promote all warnings to errors.

If this step fails, the VM exits immediately. There is no point running tests against a binary that does not exist.

### Step 3 — Integration smoke test

```yaml
- name: Run a basic test
  run: ./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
```

This invokes the compiled binary against the bundled example files. It exercises the full pipeline end-to-end: `parse_ship_config` reads `sample_ship.cfg`, `parse_cargo_list` reads `sample_cargo.txt`, `place_cargo_3d` runs the 3D bin-packer, `perform_analysis` computes stability, and `print_loading_plan` writes the human-readable report to stdout.

If any of those modules crash, return an error exit code, or trigger a sanitizer fault (when running under ASan), this step fails.

!!! note
    The digest describes a more comprehensive test suite with 8 dedicated test binaries (`test_parser`, `test_analysis`, `test_constraints`, etc.). Those are run locally via `make test` or `make test-asan`. The CI workflow currently runs only the integration smoke test. Expanding the workflow to include `make test` would be a straightforward addition — replace `./cargoforge examples/...` with `make test`.

---

## Why every push, not just releases

Consider what `perform_analysis` does: it accumulates cargo weights, calls into `hydrostatics.c` for KB and BM, calls `tanks.c` for free-surface correction, and assembles the final $GM$ value:

$$GM_{corrected} = KB + BM - KG - FSC$$

A one-character typo in the accumulation loop — say, `+=` changed to `-=` — would produce a plausible-looking number for most inputs. Manual review might not catch it. A CI run that includes a regression test with a known-correct answer catches it instantly.

The fuzzer (`scripts/fuzz.sh`) documents exactly this class of risk. It feeds adversarial inputs — negative weights, overflow values, malformed DG strings — and checks that the binary never crashes (exit code ≥ 128). The heap-use-after-free bug described in the digest was found precisely this way: `parse_cargo_list` freed `ship->cargo` on an error path but left `ship->cargo_count` non-zero, so `ship_cleanup` later walked off freed memory. AddressSanitizer, running inside the fuzzer, produced exit code 134 (SIGABRT), and the fuzzer flagged it as a fail. The fix — setting both `ship->cargo = NULL` and `ship->cargo_count = 0` immediately after the free — was validated by re-running the same adversarial inputs until they all passed cleanly.

Running that kind of gate automatically on every push means regressions are caught at the commit that introduced them, not discovered weeks later by a different developer.

---

## Reading the status in a pull request

When a contributor opens a pull request against `main`, GitHub runs the `build` job and displays the result inline on the PR page. The three possible states:

| Symbol | Meaning |
|--------|---------|
| Yellow circle | Pipeline is still running |
| Green check | All steps passed; the branch is safe to merge (from the CI perspective) |
| Red cross | At least one step failed; the PR should not be merged until fixed |

Clicking the red cross shows which step failed and the complete stdout/stderr of the failed command — enough information to reproduce the failure locally.

---

## Extending the workflow

The current `.github/workflows/c-build.yml` covers the compile + smoke-test gate. The Makefile exposes several targets that a more thorough workflow could invoke:

| Makefile target | What it adds |
|-----------------|-------------|
| `make test` | Runs all 8 unit-level test binaries |
| `make test-asan` | Rebuilds with `-fsanitize=address,undefined` and runs the full suite — catches the class of bug described in §7 of the digest |
| `make test-valgrind` | Runs each test binary under Valgrind for leak and memory-error detection |
| `make validate` | Builds and runs the DNV-SE-0475 benchmark validation |

Adding a step like:

```yaml
- name: Run test suite with AddressSanitizer
  run: make test-asan
```

would make the CI gate equivalent to the local sanitizer workflow. The compile time increases because ASan rebuilds all objects, but on a fresh VM that cost is paid unconditionally anyway.

!!! tip
    `make test-asan` is the highest-value single addition to this workflow. It replays every unit test with memory-error detection enabled and would have caught the heap-use-after-free in `parse_cargo_list` at the pull-request stage rather than requiring a dedicated fuzzer run.

---

## Recap

- CI runs automatically on every push and pull request to `main`, on a neutral Ubuntu VM with no local environment assumptions.
- `.github/workflows/c-build.yml` encodes three sequential gates: checkout → `make` (compile) → `./cargoforge examples/...` (integration smoke test).
- Each step must exit 0; any non-zero exit aborts the remaining steps and marks the job failed.
- The `runs-on: ubuntu-latest` VM is provisioned fresh each time, so "works on my machine" is never a valid excuse if CI is red.
- The Makefile's `test-asan` target — not yet wired into CI — would add AddressSanitizer coverage and would have caught the `parse_cargo_list` heap-use-after-free automatically.
- Expanding CI to run `make test` or `make test-asan` requires adding a single step to the workflow YAML.

*Next: [Lab 4 - Run and extend the tests](lab-04-build-test.md).*
