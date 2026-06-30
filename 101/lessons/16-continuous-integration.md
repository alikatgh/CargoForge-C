# Continuous integration

Every time a developer pushes code to CargoForge-C, a server on the internet automatically compiles the project, runs every test, and reports whether anything broke — before a single human reviewer reads a line. That automated pipeline is **continuous integration** (CI). This lesson explains what CI is, why it matters, and how CargoForge-C's workflow file encodes the exact gates every push must pass.

## The mental model 🧠

CI is a tireless gatekeeper standing between every push and the `main` branch. The instant you push, a brand-new machine — one that has never seen your laptop's quirks — checks out your code, builds it, runs every test, and then runs them *again* under AddressSanitizer. Only if every gate turns green does the change get to call itself "passing." No human has to remember to run anything.

The "brand-new machine" detail is the whole point. "Works on my machine" stops being an excuse when the build happens on a fresh VM with none of your local environment baked in — it is the reproducibility argument from Lesson 04, now automated and enforced on every commit. And the `make test-asan` gate is the one that earns its keep here: it is the step that would have caught the `parse_cargo_list` use-after-free before it could ever reach `main`.

<svg viewBox="0 0 600 168" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>CI runs a sequence of gates on every push; all must pass to merge</title>
<desc>A push triggers a fresh Ubuntu VM that checks out the code, builds it, runs the tests, then re-runs them under AddressSanitizer. Only if every gate passes does the change merge to main; any failing gate blocks the merge.</desc>
<text x="40" y="40" font-size="10" fill="currentColor" opacity="0.7" font-family="var(--md-code-font,monospace)">git push  →</text>
<g font-size="9.5" text-anchor="middle">
<rect x="40" y="52" width="96" height="48" rx="5" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-opacity="0.4"/><text x="88" y="74" fill="currentColor">fresh VM</text><text x="88" y="88" fill="currentColor" opacity="0.55">ubuntu-latest</text>
<rect x="148" y="52" width="96" height="48" rx="5" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-opacity="0.4"/><text x="196" y="74" fill="currentColor" font-family="var(--md-code-font,monospace)">make</text><text x="196" y="88" fill="currentColor" opacity="0.55">build ✓</text>
<rect x="256" y="52" width="96" height="48" rx="5" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-opacity="0.4"/><text x="304" y="74" fill="currentColor" font-family="var(--md-code-font,monospace)">make test</text><text x="304" y="88" fill="currentColor" opacity="0.55">unit ✓</text>
<rect x="364" y="52" width="100" height="48" rx="5" fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1.2"/><text x="414" y="72" fill="currentColor" font-family="var(--md-code-font,monospace)">test-asan</text><text x="414" y="88" fill="#12A594" opacity="0.95">catches UAF</text>
<rect x="476" y="52" width="96" height="48" rx="5" fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1.2"/><text x="524" y="74" fill="currentColor">merge ✓</text><text x="524" y="88" fill="currentColor" opacity="0.6">to main</text>
</g>
<g stroke-opacity="0.5" stroke="currentColor">
<line x1="136" y1="76" x2="146" y2="76"/><line x1="244" y1="76" x2="254" y2="76"/><line x1="352" y1="76" x2="362" y2="76"/><line x1="464" y1="76" x2="474" y2="76"/>
</g>
<text x="306" y="130" font-size="10" text-anchor="middle" fill="#D05663" opacity="0.9">✗ any failing gate blocks the merge — no human review needed</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Continuous integration (CI)** = "a robot that compiles and tests your code automatically every time you push" — it catches breakage the moment it is introduced, not weeks later when a user reports a crash.
- **Trigger (`on: push / pull_request`)** = "the list of events that wake the robot up" — CargoForge-C's workflow only fires on pushes and pull requests aimed at `main`, so experimental branches stay out of the gate.
- **`runs-on: ubuntu-latest`** = "a brand-new Linux machine that has never seen your laptop's quirks" — if the code fails there, "works on my machine" is never a valid excuse, because the VM has no local environment assumptions baked in.
- **Integration smoke test (`./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt`)** = "run the whole program end-to-end on known-good files" — it exercises `parse_ship_config`, `parse_cargo_list`, `place_cargo_3d`, `perform_analysis`, and `print_loading_plan` in one shot, so a crash anywhere in that chain fails the pipeline.
- **Non-zero exit code** = "any step that returns failure immediately kills the remaining steps" — if `make` cannot produce the `cargoforge` binary, the smoke-test step is skipped automatically; there is nothing to run.
- **`make test-asan`** = "rebuild everything with AddressSanitizer and replay every unit test" — this is the gate that would have caught the heap-use-after-free in `parse_cargo_list` (freed `ship->cargo` but left `ship->cargo_count` non-zero) before the bug ever reached `main`.

**Why it matters:** a one-character typo in the $GM$ accumulation loop — `+=` becoming `-=` — produces a plausible-looking stability number that manual review easily misses; CI with a regression test catches it on the exact commit that introduced it. Without CI, bugs from parallel changes (stability fix meets new parsing feature) hide until a user reports a crash weeks later.

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

The fuzzer ([`scripts/fuzz.sh`](https://github.com/alikatgh/CargoForge-C/blob/main/scripts/fuzz.sh)) documents exactly this class of risk. It feeds adversarial inputs — negative weights, overflow values, malformed DG strings — and checks that the binary never crashes (exit code ≥ 128). The heap-use-after-free bug described in the digest was found precisely this way: `parse_cargo_list` freed `ship->cargo` on an error path but left `ship->cargo_count` non-zero, so `ship_cleanup` later walked off freed memory. AddressSanitizer, running inside the fuzzer, produced exit code 134 (SIGABRT), and the fuzzer flagged it as a fail. The fix — setting both `ship->cargo = NULL` and `ship->cargo_count = 0` immediately after the free — was validated by re-running the same adversarial inputs until they all passed cleanly.

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
