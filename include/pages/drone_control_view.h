#pragma once

#include <memory>
#include <vector>
#include <cstdint>

#include "state/item.h"
#include "state/game.h"
#include "pages/fleet_movement.h"

extern "C"
{
#include "raylib.h"
}

class Craft;
class Location;
class Orbital;

enum DroneControlState
{
    DCS_MANAGE,
    DCS_BATTLE,
    DCS_MAX
};

// A rendered drone fleet. Owns all shared fleet state (kinematics, per-slot sim outputs
// with a live-marker array, and packed render buffers) plus a pluggable MovementPattern
// strategy that evolves the geometry. Members can be destroyed via killRandom/setLiveCount;
// dead slots are holes in the sim arrays and are dropped by compact() before rendering.
struct DroneFleetMarkers
{
    Color color;

    // shared kinematics (formerly duplicated inside each MovementPattern)
    DroneFleetState state = DFS_APPROACHING;
    float base_radius = 0.0f, sphere_radius = 0.0f;
    float engage_timer = 0.0f, engage_x = 0.0f, approach_dir = 1.0f;
    Vector2 fleet_position = {0.0f, 0.0f};

    // slot-indexed sim outputs + live flags; packed outputs for the renderer.
    // capacity == initial fleet size (fleets only shrink); buffers sized once in initialise().
    int capacity = 0, live_count = 0, out_count = 0;
    std::vector<uint8_t> live;
    std::vector<Vector2> slot_pos, out_pos;
    std::vector<float> slot_depth, out_depth;
    std::vector<int> scratch_idx; // reused scratch for killRandom

    std::unique_ptr<MovementPattern> motion;

    void initialise(int count, Color c, MovementPatternType type);
    void setApproach(Vector2 start, float target_x, float dir);
    void update(float delta);
    void killRandom(int k);
    void setLiveCount(int n) { killRandom(live_count - n); }
    void compact();
    void render(int top, int left);
};

class DroneControlView
{
    Orbital *current_orbital;
    int fleet_drone_count;
    int orbital_drone_count;
    float timer; // animation timer

    DroneFleetMarkers attackers;
    DroneFleetMarkers defenders;

    void render_manage();
    void render_battle();

public:
    Craft *craft;
    Location *location;
    ItemType droneType;
    bool visible;
    DroneControlState state;
    int top;
    int left;

    DroneControlView(int l, int t) : craft(nullptr), droneType(ItemType::MAX_ITEM_TYPE), visible(false), state(DCS_MANAGE), top(t), left(l) {}

    void activate(Craft *c);
    void deactivate();

    void input();
    void render();
    void update(float delta);
};