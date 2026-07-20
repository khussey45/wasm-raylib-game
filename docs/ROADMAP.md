# Roadmap

Each milestone should leave the game playable (natively and in the browser) and be small enough to review in one sitting.

## v0.1 — Survival Screen (current)

A single screen exercising the dodging pillar.

- [x] Ship entity: position, velocity, drawn on screen
- [x] Ship movement: fly in all directions (keyboard), clamped to screen
- [x] Obstacles: spawn off-screen, fly toward the ship, despawn when past
- [x] Collision: ship vs. obstacle ends the run (game-over state, restart key)
- [x] Score: increases with survival time, shown on screen; final score on game over
- [x] Difficulty ramp: obstacle speed and/or spawn rate increase over time

Done when: you can play a full loop — start, dodge, die, see score, restart — in the browser.

## v0.2 — Feel & Polish

- [x] Ship handling: acceleration + drag instead of raw velocity
- [x] Visual feedback: hit flash, death particle burst, screen shake
- [x] High score persisted across launches (file natively, localStorage on web)
- [x] Pause (P)

## v0.3 — Phase Switching (current)

The run alternates between the two dodging styles from the product vision:

- [x] Flight phase: the existing survival gameplay, for a fixed stretch
- [x] Transition: gravity kicks in, the ship drops to the ground (telegraphed with a warning)
- [x] Ground phase: Geometry-Dash-like — ship auto-runs on the ground, jump over spikes
- [x] Transition back: thrusters come back online, return to flight
- [x] Score and difficulty ramp continue across phases

## v0.4 — Combat

- [x] Ship can shoot (Space, flight phase only)
- [x] First enemy type: sine-wave drifter, 3 HP, destructible, rams for damage
- [x] Health (3 HP) with brief invulnerability instead of one-hit death
- [x] Score = survival time + kill bonuses
- [x] Tuning: slower obstacle/enemy start speeds, gradual ramp-up preserved
- [x] Ground phase ramps: safe terrain to ride up and launch off

## Later (unscoped)

- Arena sections with bosses and weapon variety
- Exploration: hidden areas, upgrades, resources
- Level/section structure connecting dodging, combat, and exploration
- Sound and music
- Menus and settings

Only v0.1 is committed scope. Later milestones are sketches — revisit and re-scope them when v0.1 is done, and update this file as milestones complete or change.
