#pragma once

extern "C"
{
#include "raylib.h"
}

#include <memory>
#include <vector> // decision - use this from STL until we know there is a problem. Can use std::unique_ptr<T[]>.

#include "state/location.h"
#include "state/string_caps.h"

// Star system
// embedded data is planets, moons etc
// Locations are data objects that relate to those
// Locations are travel targets

class System
{

public:
    System();
    ~System();

    int id; // persistence ID for consistency
    char name[NAME_MAX_LEN];
    int numPlanets;
    std::vector<float> planetDistances;
    std::vector<float> planetSizes;
    std::vector<Color> planetColors;
    std::vector<float> planetVelocities;
    std::vector<Vector2> planetPositions;
    std::vector<int> planetPrimaryIndexes; // array index of primary body for each planet, -1 if primary (e.g. star)

    // owning collection of locations
    std::vector<std::unique_ptr<Location>> locations; // e.g. planets, moons, asteroid belt, earth city, etc. populated based on the system data
    // primary location (star)
    Location *primary;

    void update(float time);

    System &setNumBodies(int numBodies);
    Location *addLocation(const char *name, LocationType type);
    void loadSolSystem();
};

typedef std::unique_ptr<System> SystemPtr;

typedef std::vector<SystemPtr> Systems;

SystemPtr createSystem(const bool asSolSystem);
