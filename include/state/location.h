#pragma once

extern "C"
{
#include "raylib.h"
}

#include <cstdint>
#include <vector>
#include <memory>

#include "state/resources.h"
#include "state/string_caps.h"
#include "state/shuttle.h"

// a location such as planet, moon, etc
// various of the standard actions are available when at a body
// special locations include Earth city and the asteroid belt, which have their own special actions and UI

// Earth city has research and training, and surface production.
// Asteroid belt has no standard actions, but enables certain tool actions

// open question: should they be hierarchical? Maybe not needed, but does make navigation tree tools easier.

enum LocationType
{
    LOCATION_TYPE_STAR = 0,
    LOCATION_TYPE_PLANET,
    LOCATION_TYPE_MOON,
    LOCATION_TYPE_ASTEROID_BELT,
    LOCATION_TYPE_EARTH_CITY,
    LOCATION_TYPE_SPACE,
    LOCATION_TYPE_MAX
};

class System; // forward declaration to avoid circular dependency
class Location;
typedef std::unique_ptr<Location> LocationPtr;
typedef std::vector<LocationPtr> Locations;

// body availability of resources - could use a bitfield or array
class LocationResources
{
public:
    // can parameterise on how available it is
    uint8_t availability[static_cast<size_t>(ResourceType::Count)];

    LocationResources();
};

class Location
{
public:
    char name[NAME_MAX_LEN];
    LocationType type;
    System *system; // the system this location is in, e.g. Sol

    // persistence IDs. Decide if this is mixing concerns, having it here makes save of state have consistent IDs.
    int id;          // unique ID for this location, used for persistence
    int primary_id;  // ID of primary body this location orbits. Persistence only; resolved to `primary` on load.

    Location *primary;                // body this one orbits, nullptr for a star or for space
    std::vector<Location *> children; // e.g. moons orbiting a planet. Not persisted; built from primary_id on load.

    // Orbital elements. These were System's parallel arrays, indexed by a per-system
    // `index` that every consumer had to keep aligned. Holding them here removes that
    // invariant and lets locations be created at runtime.
    float orbital_radius;   // distance from `primary`
    float orbital_velocity; // radians per unit game time
    float initial_angle;    // phase at time 0
    float radius;           // display radius; 0 means not drawn and not hit-testable
    Color color;
    Vector2 position; // position relative to `primary`, refreshed by System::update

    LocationResources resources;

    // location can have a shuttle, owns the instance
    ShuttlePtr shuttle;

    Location(System *s, const int id, const char *n, LocationType t);

    // Absolute position in system coordinates: this body's offset plus every
    // ancestor's. Iterative and depth-capped, so a malformed parent chain cannot
    // blow the stack the way the old recursion could.
    Vector2 resolvedPosition() const;
};
