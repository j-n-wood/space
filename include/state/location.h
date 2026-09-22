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

// What a location IS. Also the facility-kind discriminator: a facility is a kind of
// place, so it does not need a parallel enum of its own. Values 0-5 are persisted in
// bodies.type and 4/6/7 in facilities.type, so only ever APPEND here.
enum LocationType
{
    // celestial bodies
    LOCATION_TYPE_STAR = 0,
    LOCATION_TYPE_PLANET,
    LOCATION_TYPE_MOON,
    LOCATION_TYPE_ASTEROID_BELT,
    LOCATION_TYPE_EARTH_CITY, // a facility kind -- declared here since 2019, finally used
    LOCATION_TYPE_SPACE,
    // facilities
    LOCATION_TYPE_ORBITAL,           // orbital station
    LOCATION_TYPE_RESOURCE_FACILITY, // surface resource station
                                     // regions of a body -- orbit is the parent of that body's orbitals, surface of its
                                     // surface facilities, so "in orbit" is an ancestor test rather than a set of types
    LOCATION_TYPE_ORBIT,             // the space around a body: in orbit, no station
    LOCATION_TYPE_SURFACE,           // the ground: landed, no station
    LOCATION_TYPE_MAX
};

// True for the location kinds that are facilities rather than celestial bodies.
// Keep this in step when adding a facility kind.
inline bool locationIsFacility(LocationType t)
{
    return t == LOCATION_TYPE_ORBITAL || t == LOCATION_TYPE_RESOURCE_FACILITY || t == LOCATION_TYPE_EARTH_CITY;
}

// True for the celestial bodies -- the things a facility, orbit or surface hangs off.
// `space` counts: it is where a craft in transit genuinely is, and it terminates the walk.
inline bool locationIsBody(LocationType t)
{
    return t == LOCATION_TYPE_STAR || t == LOCATION_TYPE_PLANET || t == LOCATION_TYPE_MOON ||
           t == LOCATION_TYPE_ASTEROID_BELT || t == LOCATION_TYPE_SPACE;
}

inline bool locationIsSurface(LocationType t)
{
    return t == LOCATION_TYPE_SURFACE || t == LOCATION_TYPE_RESOURCE_FACILITY || t == LOCATION_TYPE_EARTH_CITY;
}

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

    ResourceType randomResourceType() const;

    LocationResources();
};

class Location
{
public:
    char name[NAME_MAX_LEN];
    LocationType type;
    System *system; // the system this location is in, e.g. Sol

    // persistence IDs. Decide if this is mixing concerns, having it here makes save of state have consistent IDs.
    int id;         // unique ID for this location, used for persistence
    int primary_id; // ID of primary body this location orbits. Persistence only; resolved to `primary` on load.

    Location *primary;                // body this one orbits, nullptr for a star or for space
    std::vector<Location *> children; // e.g. moons orbiting a planet. Not persisted; built from primary_id on load.

    // Orbital elements. These were System's parallel arrays, indexed by a per-system
    // `index` that every consumer had to keep aligned. Holding them here removes that
    // invariant and lets locations be created at runtime.
    float orbital_radius;   // distance from `primary`
    float orbital_velocity; // radians per unit game time
    float initial_angle;    // phase at time 0
    float radius;           // display radius; 0 means not drawn and not hit-testable

    // An asteroid belt reads the two radii differently: it is drawn as a ring about its
    // primary, so `orbital_radius` is the middle of that ring and `radius` its half-width.
    // A belt has no disc -- its `position` is just one point on the ring.
    Color color;
    Vector2 position; // position relative to `primary`, refreshed by System::update

    LocationResources resources;

    // The shuttle based at this body. Non-owning -- Game owns it, as it owns IOS.
    // Set on the body, never on a facility, so "one shuttle per body" stays the rule
    // even once a craft's location is the facility it is docked at.
    Shuttle *shuttle{nullptr};

    Location(System *s, const int id, const char *n, LocationType t);

    // Facility derives from Location and is owned through a Location pointer.
    virtual ~Location() = default;

    // Absolute position in system coordinates: this body's offset plus every
    // ancestor's. Iterative and depth-capped, so a malformed parent chain cannot
    // blow the stack the way the old recursion could.
    Vector2 resolvedPosition() const;

    inline bool isFacility() const { return locationIsFacility(type); }
    inline bool isBody() const { return locationIsBody(type); }
    inline bool isOnSurface() const { return locationIsSurface(type); }

    // The celestial body this location belongs to: itself if it is one, otherwise the
    // nearest ancestor that is. Walks up past facility, orbit and surface. Depth-capped,
    // like resolvedPosition().
    Location *body();
    const Location *body() const;

    // A body's orbit / surface child, or nullptr. Small scan of `children` -- a body has
    // a handful, not a list.
    Location *orbit() const;
    Location *surface() const;

    // This location, or an ancestor of it, is an orbit. True for a craft in orbit and for
    // one docked at a station in that orbit, which is the question most callers mean.
    bool inOrbit() const;
};
