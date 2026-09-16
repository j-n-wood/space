#include "state/waypoint.h"
#include "state/location.h"

#include <cstdio>

const char *Endpoint::description(char *dest, size_t len) const
{
    if (location)
    {
        // The location's own name says where and what: "Mars Orbital", "Mars Surface".
        std::snprintf(dest, len, "%s", location->name);
    }
    else
    {
        std::snprintf(dest, len, "Space");
    }
    return dest;
}