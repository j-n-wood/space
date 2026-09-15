#pragma once

#include <cstdint>
#include <cstddef>

// Where a facility sits, and so what a craft must do to reach it. Purely
// positional: what a facility *is* lives in LocationType. SLOC_EARTH_CITY used to
// sit here as a class discriminator in disguise.
enum SublocationType
{
    SLOC_SURFACE,
    SLOC_ORBIT,
    SLOC_COUNT
};

extern const char *SublocationTypeName[SLOC_COUNT];

class Location;

// a fully specified location (location: body in system, sublocation: surface or orbit) used for navigation and routing
// if we allow multiple facilities at a sublocation (e.g. multiple orbitals) an index can be provided.
class Endpoint
{
public:
    Location *location;
    SublocationType sublocation;
    bool docked; // endpoint is docked at a facility

    Endpoint() : location{nullptr}, sublocation{SLOC_ORBIT}, docked{false} {}

    explicit Endpoint(Location *loc, SublocationType sloc, bool d) : location{loc},
                                                                     sublocation{sloc}, docked{d} {}

    // for display and debugging, not persisted
    const char *description(char *dest, size_t len) const;
};