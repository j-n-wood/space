#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "state/resources.h"

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
    LOCATION_TYPE_EARTH_CITY
};

enum SublocationType
{
    SLOC_SURFACE,
    SLOC_ORBIT,
    SLOC_COUNT
};

class System; // forward declaration to avoid circular dependency
class Location;

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
    std::string name;
    LocationType type;
    System *system; // the system this location is in, e.g. Sol

    // persistence IDs. Decide if this is mixing concerns, having it here makes save of state have consistent IDs.
    uint32_t id;         // unique ID for this location, used for persistence
    uint32_t primary_id; // ID of primary body this location orbits

    std::vector<Location *> children; // e.g. moons orbiting a planet, or cities on a planet. This is not persisted, but built in memory based on the primary_id relationships

    LocationResources resources;

    Location(System *s, const char *n, LocationType t);
};