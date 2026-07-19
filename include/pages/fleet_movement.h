#pragma once

#include <memory>

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

struct DroneFleetMarkers;

// Abstract fleet motion, as a pure strategy. The owning DroneFleetMarkers holds all
// shared fleet state (kinematics, live-marker + output buffers); a pattern carries only
// its own geometry and evolves the owner's per-slot positions/depths through these hooks.
// Concrete patterns are created via makeMovementPattern() and swapped behind this interface.
class MovementPattern
{
public:
    virtual ~MovementPattern() = default;

    // Seat pattern geometry once the fleet's approach layout is configured.
    virtual void onConfigure(DroneFleetMarkers &f) {}

    // Per-phase evolution. Each writes live slots' f.slot_pos / f.slot_depth.
    virtual void stepApproach(DroneFleetMarkers &f, float delta) = 0;
    virtual void onBeginEngage(DroneFleetMarkers &f) {}
    virtual void stepEngage(DroneFleetMarkers &f, float delta) = 0;
    virtual void stepRetreat(DroneFleetMarkers &f, float delta) = 0;

    // Runs after the phase step in every phase (Flocking updates its boids here).
    virtual void postStep(DroneFleetMarkers &f, float delta) {}
};

std::unique_ptr<MovementPattern> makeMovementPattern(MovementPatternType type, int count);
