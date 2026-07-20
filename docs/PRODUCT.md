# Product Vision

A 2D sci-fi exploration game. The player pilots a small ship through increasingly dangerous environments. Some sections are high-speed obstacle courses; others open into arenas where the player fights enemies and explores.

## Core Concept

One control scheme (flying the ship) supports three kinds of gameplay:

```
        CORE: Spaceship controls
                 │
        ┌────────┼────────┐
        ↓        ↓        ↓
    DODGING    COMBAT   EXPLORATION
        │        │        │
   Geometry-   enemies   hidden areas
   Dash-like   bosses    upgrades
   sections    weapons   resources
        └────────┼────────┘
                 ↓
          One cohesive game
```

- **Dodging** — high-speed obstacle sections in the spirit of Geometry Dash: reflex-driven, rhythm of near-misses, escalating speed.
- **Combat** — arenas with enemies, bosses, and weapons.
- **Exploration** — hidden areas, upgrades, and resources that reward curiosity.

The three pillars share the same ship, physics, and controls, so mastery transfers between them and the game feels like one thing rather than three minigames.

## Version 0.1 — Survival Screen

The first playable version is deliberately minimal: a single screen exercising the dodging pillar.

Requirements:

1. The ship can fly in all directions.
2. Obstacles fly toward the ship.
3. Colliding with an obstacle ends the run.
4. Score increases with survival time.
5. Difficulty gradually increases (obstacle speed and/or spawn rate).

Everything else — combat, exploration, levels, upgrades, menus — is explicitly out of scope for 0.1. See `docs/ROADMAP.md`.

## Platforms

- Native desktop (macOS today)
- Browser (WebAssembly via Emscripten) — the game must stay playable in the browser at every milestone.
