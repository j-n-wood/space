# Space

Space colonization strategy game. Manage orbital stations and surface facilities, extract resources, and produce items across the Sol system.

Based on Deuteros.

Built with C++20, Raylib, and SQLite3.

## Prerequisites

- A C++20 compiler (clang or g++14)
- SQLite3 development libraries (Linux: `libsqlite3-dev`, macOS: provided by the system)
- Git (for submodule checkout)

Premake5 binaries are bundled in `build/` — no separate install needed.

## Getting Started

Clone the repository with submodules:

```bash
git clone --recurse-submodules <repo-url>
cd space
```

If you already cloned without `--recurse-submodules`:

```bash
git submodule update --init
```

Configure and build:

```bash
./reconf.sh        # generates Makefiles via premake5
make -j10           # debug build (default: debug_x64)
./bin/Debug/space   # run the game
```

## Build Commands

```bash
# Configure (only needed once, or after editing premake5.lua)
./reconf.sh

# Build
make                           # debug_x64 (default)
make config=release_x64        # release build
make config=debug_arm64        # debug ARM64 (Apple Silicon)
make config=release_arm64      # release ARM64

# Run
./bin/Debug/space              # debug build
./bin/Release/space            # release build

# Tests
make tests && ./bin/Debug/tests
./bin/Debug/tests --test-case="*Database*"   # run specific tests

# Clean
make clean && make
```

## Managing the Raylib Submodule

Raylib lives in `build/external/raylib` as a git submodule.

```bash
# Update to latest upstream
cd build/external/raylib
git fetch
git checkout v5.5              # or a specific tag/commit
cd ../../..
git add build/external/raylib
git commit -m "update raylib to v5.5"

# After pulling changes that update the submodule pointer
git submodule update
```

## Build System

The build uses Premake5 to generate GNU Makefiles. `premake5.lua` is at the project root. Generated `.make` files go into `build/`.

```bash
# Regenerate makefiles (after editing premake5.lua)
./reconf.sh

# Generate compile_commands.json for clangd / IDE support
./build/premake5.osx ecc
```

### Premake options

```bash
# OpenGL version (default: opengl33)
./build/premake5.osx gmake --graphics=opengl21

# Platform backend (default: glfw)
./build/premake5.osx gmake --backend=rgfw

# Wayland support on Linux (default: off)
./build/premake5 gmake --wayland=on
```

## Project Structure

```
premake5.lua          # build configuration
src/                  # source files
include/              # header files
  state/              # game state (simulation, no UI dependency)
  pages/              # UI pages (raylib rendering)
  loaders/            # SQLite persistence
  assets/             # texture management
tests/                # doctest test files
resources/            # game assets and initial.db
build/                # premake5 binaries, generated makefiles, ecc module
  external/raylib/    # raylib submodule
bin/                  # build output (Debug/ and Release/)
```
