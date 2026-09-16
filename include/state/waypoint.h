#pragma once

#include <cstdint>
#include <cstddef>

// Which side of a body: what a craft must do to reach something there. Purely
// positional -- what a facility *is* lives in LocationType.
enum SublocationType
{
    SLOC_SURFACE,
    SLOC_ORBIT,
    SLOC_COUNT
};

extern const char *SublocationTypeName[SLOC_COUNT];

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