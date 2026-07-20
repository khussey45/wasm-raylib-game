# Architecture

## Overview

- **Language:** C++23
- **Framework:** raylib 5.5, fetched automatically via CMake `FetchContent` (no manual install)
- **Build system:** CMake ≥ 3.20
- **Targets:**
  - Native desktop (macOS) — `build/`
  - Browser (WebAssembly via Emscripten) — `build-web/`

## Source Layout

```
src/main.cpp     — everything, for now
docs/            — product vision, architecture, roadmap
CMakeLists.txt   — one target, native + web configuration
```

The game currently lives in a single translation unit. This is intentional at this stage: it keeps the whole system readable in one sitting. Split files only when a milestone makes `main.cpp` genuinely hard to navigate, and split along responsibilities (e.g. `game state / update / render`), not speculative layers.

## The Frame Loop (important)

The web build is the reason for the loop's shape. Browsers cannot run a blocking `while` loop — Emscripten drives one frame per `requestAnimationFrame` tick instead. So:

- All per-frame logic lives in `UpdateDrawFrame()`.
- Game state that must persist across frames lives at file scope (a browser frame call can't keep locals alive between frames).
- `main()` only initializes, then either hands `UpdateDrawFrame` to `emscripten_set_main_loop` (web) or calls it in a `while (!WindowShouldClose())` loop (native), selected by `#if defined(PLATFORM_WEB)`.

**Do not reintroduce a blocking game loop or move persistent state into `main()`'s locals — it will break the browser build.**

Frame pacing: native uses `SetTargetFPS(60)`; the browser uses the display's refresh rate via rAF. Always scale movement by `GetFrameTime()` (delta time), never assume a fixed frame duration.

## Build Configuration

`CMakeLists.txt` has one executable target and two conditional blocks:

- `if (EMSCRIPTEN)` — sets raylib's `PLATFORM=Web`, defines `PLATFORM_WEB`, links `-sUSE_GLFW=3`, emits `.html/.js/.wasm`.
- `if (APPLE)` — links macOS frameworks for the native build.

### Build commands

Native:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/game-raylib
```

Web (requires Emscripten, installed via Homebrew):

```bash
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python3 -m http.server 8000 -d build-web   # wasm won't load from file://
# open http://localhost:8000/game-raylib.html
```

The configure step only needs rerunning when `CMakeLists.txt` changes; day-to-day it's just `cmake --build`.

## Assets

There are none yet. When assets are added, the web build will need Emscripten's `--preload-file` to bundle them into the virtual filesystem — note this here when it happens.

## Testing

No automated tests yet. Game logic that becomes non-trivial (collision, spawning, scoring) should be written as pure functions over plain data where practical, so it can be unit-tested without raylib.
