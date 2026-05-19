#include "loaders/loader.h"
#include "state/location.h"

const char *SublocationTypeName[SLOC_COUNT] = {
    "Surface",
    "Orbital",
    "Earth City",
};

Location::Location(System *s, const int lid, const char *n, LocationType t) : system(s), id(lid), type(t)
{
    copyFixed(name, sizeof name, n);
}

LocationResources::LocationResources()
{
    for (int idx = 0; idx < ResourceType::Count; ++idx)
    {
        availability[idx] = 0;
    }
}