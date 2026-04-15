# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Space colonization strategy game (C++20, Raylib, SQLite3). Inspired by Utopia — manage orbital/surface facilities, extract resources, produce items across the Sol system.

## Build Commands

Build system is Premake5 → GNU Make. Raylib is a git submodule in `build/external/raylib`.

```bash
# Configure (only needed once, or after editing premake5.lua)
./reconf.sh                    # runs: build/premake5.osx gmake

# Build
make                           # debug_x64 (default)
make config=release_x64        # release build

# Run
./bin/debug_x64/space

# Tests (doctest framework)
make tests && ./bin/debug_x64/tests
# Run specific tests by name
./bin/debug_x64/tests --test-case="*Database*"

# Clean rebuild
make clean && make
```

On Linux, install `libsqlite3-dev`. On macOS, sqlite3 links via framework automatically.

## Architecture

### Separation of simulation and presentation

Game state runs independently of UI. State classes have no knowledge of Raylib or rendering. Pages read state and render it. This means the simulation could run headless.

**Data flow:** `Game::update(delta)` → Pages read state → Raylib renders → Page handles input → loop

### Core state model (`include/state/`, `src/state/`)

- **Game** — singleton owning all state. Holds current system/location/facility (UI focus), owns collections of systems, bases, orbitals. Manages factory references (non-owning).
- **System** — contains a star (primary) with child Locations (planets, moons, asteroid belt). Updates orbital positions for animation.
- **Location** — a body (star, planet, moon, asteroid belt, earth city). Has a type, available resources, and optional children (e.g. Earth has moons, cities).
- **Facility** (abstract) → **ResourceFacility** (surface mining) and **Orbital** (orbital station). Each has Stores (inventory) and optionally a Factory.
- **Factory** — production queue. Processes items tick-by-tick, checking tech level and resource costs.
- **Stores** — resource inventory (ResourceType → quantity). Attached to facilities.
- **Item** — item definitions loaded from database at startup (name, build time, costs, tech level).

### UI system (`include/pages/`, `src/pages/`)

- **PageManager** — singleton, owns all page instances, switches between them.
- **BasePage** (abstract) — provides standard layout (sidebar + main screen). Uses a `standardButtons` bitmask to show/hide context-sensitive buttons. Virtual methods: `activate()`, `render()`, `input()`.
- Pages: SystemView, EarthCity, StoresView, FactoryView, ResourcesView, plus shuttle/production views.
- **Overlay** — singleton rendered on top of all pages (time display, tooltips). Reset each frame via `start()`.

Button visibility is driven by location context, not page type. EC (Earth Command) has training/research buttons; other locations show orbit/surface controls.

### Persistence (`include/loaders/`, `src/loaders/`)

SQLite-based. The initial game state lives in `resources/initial.db`.

- **Loader** — reads SQLite, populates a Game object (systems, locations, facilities, stores, items).
- **SaveGame** — serializes Game state back to SQLite.
- **SQLiteQuery** — wrapper for prepared statements with chained bind/step.

Quicksave/quickload: F5 saves to `quicksave.db`, F8 loads by creating a fresh Game and swapping it in.

### Assets (`include/assets/`)

**TextureManager** — singleton cache for Raylib textures and sprite sheet coordinates. Must call `dispose()` before window close.

## Key Conventions

- Singletons for global managers (Game, PageManager, TextureManager, Overlay)
- `unique_ptr` ownership for game objects; raw pointers for non-owning references
- Forward declarations to avoid circular includes
- Static layout rectangles on BasePage — call `setWindowSize()` to update
- Premake5 config in `build/premake5.lua`; generated makefiles in `build/build_files/`
- C++20 standard, C17 for C code (Raylib)

## Adding a New Page

1. Header in `include/pages/`, implementation in `src/pages/` (extend BasePage)
2. Add enum value to `Page` in `include/pages/pages.h`
3. Register in PageManager constructor
4. Implement `activate()`, `render()`, `input()`
5. Set `standardButtons` bitmask for context controls

## Adding Game State

1. Class in `include/state/`, implementation in `src/state/`
2. Add owning reference in Game (usually `unique_ptr` or `vector`)
3. Add load/save in Loader and SaveGame
4. Database schema changes go in `resources/initial.db`

## In-Game Hotkeys

- **1/2**: Switch pages (System View / Earth City)
- **Space**: Pause/resume time
- **F5/F8**: Quicksave/Quickload
