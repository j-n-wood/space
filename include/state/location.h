#pragma once

#include <cstdint>
#include <string>

// a location such as planet, moon, etc
// various of the standard actions are available when at a body
// special locations include Earth city and the asteroid belt, which have their own special actions and UI

// Earth city has research and training, and surface production.
// Asteroid belt has no standard actions, but enables certain tool actions

enum LocationType {
    LOCTION_TYPE_STAR = 0,
    LOCATION_TYPE_PLANET,
    LOCATION_TYPE_MOON,
    LOCATION_TYPE_ASTEROID_BELT,
    LOCATION_TYPE_EARTH_CITY
};

class System; // forward declaration to avoid circular dependency

class Location {
public:
    std::string name;
    LocationType type;
    System* system; // the system this location is in, e.g. Sol

    Location(System* s, const char* n, LocationType t);
};