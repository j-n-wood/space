#pragma once

#include <cstdint>

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

    explicit Endpoint(Location *loc, SublocationType sloc, bool docked) : location{loc},
                                                                          sublocation{sloc}, docked{docked} {}
};