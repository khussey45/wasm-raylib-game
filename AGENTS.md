# AGENTS.md

## About This Project

A 2D sci-fi exploration game built in C++23 with raylib. The player pilots a small ship through increasingly dangerous environments: high-speed dodging sections, combat arenas, and explorable areas. See `docs/PRODUCT.md` for the full vision and `docs/ROADMAP.md` for what is currently being built.

Current focus: **v0.1 — a single survival screen** (ship flies in all directions, obstacles fly toward it, collision ends the run, score = survival time, difficulty ramps up).

Key facts:

- Language/toolkit: C++23, raylib 5.5 (fetched via CMake FetchContent)
- Targets: native macOS (`build/`) and browser via Emscripten/WASM (`build-web/`)
- Build commands and architecture: see `docs/ARCHITECTURE.md`
- The game loop lives in `UpdateDrawFrame()` in `src/main.cpp` — this shape is required for the web build (`emscripten_set_main_loop`); do not reintroduce a blocking loop.

## Project Goal

Build this project incrementally while keeping the codebase easy to understand, test, and maintain.

The developer is using this project to improve as an engineer while shipping a real product.

Do not optimize only for speed. Optimize for:

- clarity
- correctness
- maintainability
- learning
- small, reviewable changes

## Development Workflow

Work in small milestones.

For each task:

1. Read the relevant project documentation before making changes.
2. Understand the existing architecture and conventions.
3. Make the smallest reasonable implementation that satisfies the current requirement.
4. Do not implement future roadmap items unless they are required by the current task.
5. Run relevant tests, linters, and formatters.
6. Summarize what changed after implementation.
7. Explain any important architectural decisions.

Do not make large unrelated refactors while implementing a feature.

## Learning-Oriented Development

The developer wants to understand the system being built.

Prefer code that is:

- explicit rather than overly clever
- easy to trace and debug
- idiomatic for the language/framework
- organized around clear responsibilities

Avoid unnecessary abstractions.

Do not introduce a pattern, library, service, or dependency unless it solves a concrete problem.

When introducing an important new concept, briefly explain:

- why it is needed
- where it fits in the architecture
- what alternatives were considered

## Planning

Before implementing a non-trivial feature:

1. Identify the files likely to change.
2. Identify any architectural implications.
3. Break the work into small steps.
4. Note important risks or assumptions.

Prefer implementing one coherent milestone at a time.

## Code Quality

Follow the existing style of the repository.

General rules:

- Use meaningful names.
- Keep functions and methods focused.
- Avoid duplicated logic.
- Handle errors explicitly.
- Validate external input.
- Keep business logic separate from transport/UI concerns where practical.
- Prefer simple solutions over premature abstraction.

## Testing

Add or update tests when behavior changes.

Before considering work complete:

- run relevant automated tests
- run formatting tools
- run linters if configured
- verify the main behavior manually when appropriate

Do not delete or weaken tests simply to make them pass.

## Dependencies

Avoid adding dependencies when the standard library or existing project dependencies are sufficient.

Before adding a dependency, consider:

- maintenance activity
- security implications
- complexity added
- whether the functionality is simple enough to implement locally

Explain significant new dependencies.

## Documentation

Keep documentation synchronized with meaningful architectural changes.

Update:

- `README.md` when setup or usage changes
- `docs/ARCHITECTURE.md` when architecture changes
- `docs/ROADMAP.md` when milestones are completed or changed
- `docs/PRODUCT.md` only when product requirements change

Do not create unnecessary documentation files.

## Git Changes

Keep changes focused.

Avoid:

- modifying unrelated files
- formatting the entire repository unnecessarily
- rewriting working code without a clear reason

A good change should be easy for another engineer to review.

## Completion Summary

After completing a task, provide:

### What changed

A concise explanation of the implementation.

### Files changed

List important files and their purpose.

### How it works

Explain the execution flow in simple terms.

### Tests

Explain what was tested and whether it passed.

### Things to study

Identify the most important code or concepts the developer should understand before moving to the next milestone.

### Next logical step

State the next roadmap item, but do not implement it unless explicitly requested.