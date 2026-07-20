# Fly Fall

A 2D sci-fi exploration game: pilot a small ship through high-speed dodging sections, combat arenas, and explorable areas. Playable natively and in the browser (WebAssembly).

## Status

Early development — building **v0.1: a single survival screen** (fly, dodge incoming obstacles, survive as long as you can).

See:

- [Product](docs/PRODUCT.md) — the game vision
- [Architecture](docs/ARCHITECTURE.md) — how the code and builds are set up
- [Roadmap](docs/ROADMAP.md) — milestones

## Tech Stack

- C++23 with [raylib](https://www.raylib.com/) 5.5
- CMake (raylib fetched automatically via FetchContent)
- Emscripten for the browser/WASM build

## Requirements

- CMake ≥ 3.20 and a C++23 compiler (Xcode Command Line Tools on macOS)
- Emscripten (`brew install emscripten`) — only for the web build
- Python 3 — only to serve the web build locally

## Build & Run

Native:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/game-raylib
```

Browser:

```bash
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python3 -m http.server 8000 -d build-web
# open http://localhost:8000/game-raylib.html
```
