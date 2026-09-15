#include "loaders/loader.h"
#include "state/location.h"

const char *SublocationTypeName[SLOC_COUNT] = {
    "Surface",
    "Orbital",
};

Location::Location(System *s, const int lid, const char *n, LocationType t)
    : type(t), system(s), id(lid), primary_id(-1), primary(nullptr),
      orbital_radius(0.0f), orbital_velocity(0.0f), initial_angle(0.0f), radius(0.0f),
      color({255, 255, 255, 255}), position({0.0f, 0.0f})
{
    copyFixed(name, sizeof name, n);
}

Vector2 Location::resolvedPosition() const
{
    // Depth cap rather than trusting the parent chain: a cycle here used to be
    // unbounded recursion.
    const int MAX_DEPTH = 16;
    Vector2 p = position;
    int depth = 0;
    for (const Location *n = primary; n != nullptr && depth < MAX_DEPTH; n = n->primary, ++depth)
    {
        p.x += n->position.x;
        p.y += n->position.y;
    }
    return p;
}

LocationResources::LocationResources()
{
    for (int idx = 0; idx < ResourceType::Count; ++idx)
    {
        availability[idx] = 0;
    }
}