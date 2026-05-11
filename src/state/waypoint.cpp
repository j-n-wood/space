#include "state/waypoint.h"
#include "state/location.h"

#include <cstdio>

const char *Endpoint::description(char *dest, size_t len) const
{
    if (location)
    {
        std::snprintf(dest, len, "%s %s%s", SublocationTypeName[sublocation], location->name, docked ? " (docked)" : "");
    }
    else
    {
        std::snprintf(dest, len, "Space");
    }
    return dest;
}