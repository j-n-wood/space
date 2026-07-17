#pragma once

#include <memory>

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

// A rendered drone fleet: owns a pluggable MovementPattern that drives marker positions,
// and draws them in the fleet's colour. The motion model is chosen at initialise().
struct DroneFleetMarkers
{
    Color color;
    std::unique_ptr<MovementPattern> motion;

    void initialise(int count, Color c, MovementPatternType type);
    void setApproach(Vector2 start, float target_x, float dir);
    void update(float delta);
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