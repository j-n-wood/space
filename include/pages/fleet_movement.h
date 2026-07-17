#pragma once

#include <memory>

extern "C"
{
#include "raylib.h"
}

// Selects which motion model drives a drone fleet's markers.
enum MovementPatternType
{
    MP_HELICAL,  // every marker slides along a shared spherical-helix wire
    MP_FLOCKING  // boids members follow virtual leader points moving on the sphere
};

// Phase of an engagement, shared by all movement patterns.
enum DroneFleetState
{
    DFS_APPROACHING, // fleet centroid travels toward the battle centre
    DFS_ENGAGING,    // centroid fixed; sphere inflates and markers work its surface
    DFS_RETREATING   // fleet translates away from the battle area
};

// Abstract fleet motion. A pattern owns its own state, evolves marker positions each
// frame from a per-fleet geometry, and exposes read-only outputs for the renderer.
// Concrete patterns are created via makeMovementPattern() and swapped behind this interface.
class MovementPattern
{
public:
    virtual ~MovementPattern() = default;

    // Configure geometry once the battle-area layout is known (battle-area-local coords).
    //   start    - initial fleet centroid
    //   target_x - centroid x to reach before switching from approach to engage
    //   dir      - travel direction: +1 = left-to-right, -1 = right-to-left
    virtual void configure(Vector2 start, float target_x, float dir) = 0;

    virtual void update(float delta) = 0;

    virtual int count() const = 0;                // number of rendered markers
    virtual const Vector2 *positions() const = 0; // battle-area-local marker positions
    virtual const float *depths() const = 0;      // 0..1 nearness, for the render size cue
    virtual DroneFleetState phase() const = 0;
};

std::unique_ptr<MovementPattern> makeMovementPattern(MovementPatternType type, int count);
