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

// The state a craft is asked to END UP in on arrival. Deliberately not CraftState:
// only these four are legal destinations -- you cannot ask a craft to finish in
// transit or mid-manoeuvre -- and CraftState lives in craft.h, which includes this
// header. Replaces the old (sublocation, docked) pair, which could express the same
// four states in eight combinations.
enum EndpointState
{
    EP_ORBIT,
    EP_ORBIT_DOCKED,
    EP_SURFACE,
    EP_SURFACE_DOCKED,
    EP_COUNT
};

extern const char *EndpointStateName[EP_COUNT];

inline bool endpointWantsDocked(EndpointState s)
{
    return s == EP_ORBIT_DOCKED || s == EP_SURFACE_DOCKED;
}

inline SublocationType endpointSublocation(EndpointState s)
{
    return (s == EP_ORBIT || s == EP_ORBIT_DOCKED) ? SLOC_ORBIT : SLOC_SURFACE;
}

inline EndpointState endpointStateFor(SublocationType sublocation, bool docked)
{
    if (sublocation == SLOC_ORBIT)
    {
        return docked ? EP_ORBIT_DOCKED : EP_ORBIT;
    }
    return docked ? EP_SURFACE_DOCKED : EP_SURFACE;
}

// A travel target: where to go, and what to be doing on arrival.
class Endpoint
{
public:
    Location *location;
    EndpointState state;

    Endpoint() : location{nullptr}, state{EP_ORBIT} {}

    explicit Endpoint(Location *loc, EndpointState s) : location{loc}, state{s} {}

    // for display and debugging, not persisted
    const char *description(char *dest, size_t len) const;
};