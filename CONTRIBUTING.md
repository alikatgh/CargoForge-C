# Contributing to CargoForge-C

Thank you for your interest in contributing to CargoForge-C! This project is an open-source effort to build a pure C simulator for maritime cargo logistics, emphasizing performance, hardware efficiency, and real-world applicability to challenges like ship stability and route optimization. As a learning-focused project, we value contributions from C enthusiasts, embedded systems developers, and logistics/maritime experts.

Our goal is to keep the codebase modular, portable, and dependency-free while addressing practical problems in global shipping. Before diving in, please review this guide to ensure your contributions align with the project's vision.

## Code of Conduct
We follow a simple code of conduct: Be respectful, collaborative, and constructive. Discrimination or harassment will not be tolerated. Report issues to the maintainers via GitHub issues.

## How to Contribute
- **Find or Suggest Tasks**: Check open issues, especially those labeled "good first issue" for beginners or "help wanted" for broader needs. If you have an idea (e.g., a new algorithm), open a discussion or issue first to align with the roadmap.
- **Branching**: Use descriptive branch names like `feature/stability-calc` or `fix/memory-leak`.
- **Commits**: Follow Conventional Commits style (e.g., `feat: add Dijkstra algorithm`, `fix: handle invalid input`, `docs: update README`). Keep messages concise and reference issues (e.g., `Closes #5`).
- **Pull Requests (PRs)**: 
  - Base PRs on the `main` branch.
  - Include a clear description: What does it do? Why is it needed? How was it tested?
  - Add tests or examples if applicable.
  - Ensure code compiles with `make` and passes any existing checks.
  - PRs may be squashed on merge for clean history.
- **Review Process**: Expect feedback within a week. Maintainers will review for adherence to style, performance goals, and purity (no external deps).

## Code Guidelines
To maintain consistency and the project's pure C ethos:
- **Style**: Use K&R bracing style with 4-space indentation. No tabs. Limit lines to 80 characters. Variable names in snake_case (e.g., `cargo_weight`). Functions in lowercase with underscores.
- **C Standard**: Stick to C99 for compatibility. Avoid C++ features or non-standard extensions.
- **Purity and Dependencies**: No external libraries—implement everything from scratch (e.g., custom string handling if needed). Use standard headers only (e.g., `<stdio.h>`, `<stdlib.h>`).
- **Performance Focus**: Prioritize speed and low memory use. Use inline functions, fixed-size arrays where possible, and profile with tools like `gprof`. Avoid unnecessary allocations.
- **Error Handling**: Use consistent patterns (e.g., return negative ints for errors). Add meaningful messages via `fprintf(stderr, ...)`.
- **Testing**: Write manual test cases in a `tests/` directory. No frameworks—use simple main functions to verify outputs. Test on multiple platforms (e.g., Linux, embedded sims).
- **Documentation**: Comment code generously (Doxygen-style for functions). Update README or add examples for new features.
- **Build**: All code must build with GCC/Clang via the Makefile. Add optimizations like `-O3` but ensure debuggability with `-g`.

## Current Challenges
CargoForge-C is in its early alpha stage, and we're tackling several hurdles to make it a robust tool. Contributors should be aware of these to focus efforts effectively and avoid surprises. We've listed them below, grouped by category, with potential ways to help.

### Technical Challenges
- **Portability Across Hardware**: While designed for "any" C-compatible platform, real-world testing on embedded systems (e.g., ARM boards like Raspberry Pi simulating ship computers) is limited. Cross-compilation works in theory, but endianness, pointer sizes, and peripherals vary. *Help*: Add platform-specific tests or conditional compiles (#ifdefs) without breaking purity.
- **Pure C Limitations**: Building everything from scratch (e.g., file parsers, data structures) increases complexity and bug risk. No libs means reinventing wheels like efficient sorting or graph traversal. *Help*: Optimize core utils (e.g., a fast custom qsort) or suggest pure C alternatives for common needs.
- **Performance Optimization**: Achieving "super fast" execution on low-resource hardware is key, but algorithms (e.g., cargo placement heuristics) can be computationally intensive. Profiling shows hotspots in loops or memory access. *Help*: Implement bitwise tricks, inline asm (sparingly), or parallelize where possible (though pure C limits this).
- **Memory Management**: In pure C, leaks or overflows are easy pitfalls, especially with dynamic cargo lists. Valgrind helps, but embedded envs lack such tools. *Help*: Add robust alloc/free wrappers or static analysis hooks.

### Domain-Specific Challenges
- **Maritime Accuracy**: Simulating real ship logistics (e.g., stability calcs, cargo constraints) requires validation against standards like IMO SOLAS. Current models are simplified; real data (e.g., from public datasets) is sparse. *Help*: Integrate physics formulas (e.g., center of gravity equations) or source validated ship/cargo examples.
- **Scalability for Large Ships**: Handling 20,000+ containers efficiently strains pure C arrays/graphs. Brute-force opts work for small sims but not real-scale. *Help*: Develop advanced algos like genetic or greedy heuristics tailored to maritime rules (e.g., hazardous goods separation).
- **Hardware Integration**: "Super close to hardware" is aspirational—current version is CLI-only. Interfacing with sensors (e.g., serial for GPS) needs POSIX-like abstractions without deps. *Help*: Add stubs for embedded I/O or test on actual boards.

### Project and Community Challenges
- **Early-Stage Instability**: As alpha, APIs and structures may change (e.g., ship struct redesign). Breaking changes are expected until v1.0. *Help*: Propose stable interfaces early or refactor with backward compat.
- **Testing and Validation**: No automated CI yet; manual tests are basic. Simulating ship envs (e.g., weather impacts) is incomplete. *Help*: Set up GitHub Actions for builds/tests or create simulation datasets.
- **Contributor Onboarding**: Limited docs/examples make entry hard for non-C experts. Maritime knowledge gap may deter devs. *Help*: Write tutorials, add comments, or create issue templates for domain Q&A.
- **Scope Creep**: Balancing "practical" features with pure C constraints risks overambition. E.g., adding routes without bloating code. *Help*: Prioritize modular additions (e.g., separate route module) and review against roadmap.
- **Security Considerations**: Though a simulator, logistics data could be sensitive; pure C vulnerabilities (e.g., buffer overflows) need auditing. *Help*: Add sanitizers or fuzzing setups.

By addressing these, we can evolve CargoForge-C into a valuable tool. If a challenge resonates, mention it in your issue/PR!

## Contact
- Use GitHub issues for bugs/features.
- Discussions for ideas/feedback.
- Maintainers: @alikatgh (feel free to DM on GitHub for quick chats).

Thanks for helping forge this project forward! Your contributions will make a real difference in learning C and advancing open-source logistics tech.