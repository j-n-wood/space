#pragma once

#include "state/item.h"
#include "state/game.h"

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

struct DroneMarker
{
    float x;
    float y;
    float angle; // for animation
    float cooldown;
};

enum DroneFleetState
{
    DFS_APPROACHING,
    DFS_ENGAGING,
    DFS_RETREATING
};

// movement depends on state

// in approach state, move as if points on surface of a rotating sphere
// in engage state, follow curves around midpoint of area, from whatever point they are at end of approach phase. Follow arcs around a sphere centered on battle area.
// in retreat state, move directly away from midpoint of area in the retreat direction (horizontal)

struct DroneFleetMarkers
{
    Color color;
    DroneFleetState state;
    DroneMarker fleet[MAX_DRONE_FLEET_SIZE];
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
};