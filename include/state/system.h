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

    // non-owning collection of locations
    std::vector<Location *> locations; // e.g. planets, moons, asteroid belt, earth city, etc. populated based on the system data
    // primary location (star)
    Location *primary;
    // space location (interplanetary space)
    Location *space;

    // Refresh every location's position about its primary. Orbital elements live on
    // Location, so this grows naturally as locations are added.
    void update(double time);

    inline size_t bodyCount() const { return locations.size(); }
};

typedef std::unique_ptr<System> SystemPtr;

typedef std::vector<SystemPtr> Systems;
