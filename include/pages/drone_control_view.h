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
    int size;
    float fleet_speed;
    float fleet_rotation_rate;
    Vector2 fleet_position; // position of fleet in battle area, for calculating marker positions
    float path_length[MAX_DRONE_FLEET_SIZE];
    Vector2 position[MAX_DRONE_FLEET_SIZE];

    void initialise(int count, Color c);
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