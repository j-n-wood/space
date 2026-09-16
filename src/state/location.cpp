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

namespace
{
    // Facility -> orbit/surface -> body is two hops; the cap is slack, not a limit.
    const int MAX_ANCESTOR_DEPTH = 16;
}

const Location *Location::body() const
{
    const Location *n = this;
    int depth = 0;
    while (n && !n->isBody() && depth < MAX_ANCESTOR_DEPTH)
    {
        n = n->primary;
        ++depth;
    }
    return n;
}

Location *Location::body()
{
    // one implementation, const stripped back off for the mutable caller
    return const_cast<Location *>(static_cast<const Location *>(this)->body());
}

Location *Location::orbit() const
{
    for (Location *child : children)
    {
        if (child->type == LOCATION_TYPE_ORBIT)
        {
            return child;
        }
    }
    return nullptr;
}

Location *Location::surface() const
{
    for (Location *child : children)
    {
        if (child->type == LOCATION_TYPE_SURFACE)
        {
            return child;
        }
    }
    return nullptr;
}

bool Location::inOrbit() const
{
    const Location *n = this;
    int depth = 0;
    while (n && depth < MAX_ANCESTOR_DEPTH)
    {
        if (n->type == LOCATION_TYPE_ORBIT)
        {
            return true;
        }
        if (n->isBody())
        {
            return false; // reached the body without passing through an orbit
        }
        n = n->primary;
        ++depth;
    }
    return false;
}

Vector2 Location::resolvedPosition() const
{
    // Depth cap rather than trusting the parent chain: a cycle must not hang.
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