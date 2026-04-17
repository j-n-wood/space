#include "loaders/loader.h"
#include "state/location.h"

const char *SublocationTypeName[SLOC_COUNT] = {
    "Surface",
    "Orbital",
};

Location::Location(System *s, const char *n, LocationType t) : system(s),
                                                               type(t)
{
    copyFixed(name, sizeof name, n);
    // set some default resource availability
    resources.availability[ResourceType::Iron] = 1;
    resources.availability[ResourceType::Titanium] = 1;
}

LocationResources::LocationResources()
{
    for (int idx = 0; idx < ResourceType::Count; ++idx)
    {
        availability[idx] = 0;
    }
}