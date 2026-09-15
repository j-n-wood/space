#include "state/waypoint.h"
#include "state/location.h"

#include <cstdio>

const char *EndpointStateName[EP_COUNT] = {
    "Orbit",
    "Orbit docked",
    "Surface",
    "Surface docked",
};

const char *Endpoint::description(char *dest, size_t len) const
{
    if (location)
    {
        std::snprintf(dest, len, "%s %s%s", SublocationTypeName[endpointSublocation(state)],
                      location->name, endpointWantsDocked(state) ? " (docked)" : "");
    }
    else
    {
        std::snprintf(dest, len, "Space");
    }
    return dest;
}