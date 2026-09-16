#pragma once

#include <cstdint>
#include <cstddef>

class Location;

// A travel target. The location says everything: Mars Orbital means docked there,
// Mars Orbit means in orbit with no station, Mars Surface means on the ground.
class Endpoint
{
public:
    Location *location;

    Endpoint() : location{nullptr} {}

    explicit Endpoint(Location *loc) : location{loc} {}

    // for display and debugging, not persisted
    const char *description(char *dest, size_t len) const;
};