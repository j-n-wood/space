#pragma once

#include "state/system.h"
#include <memory>

using onDestinationSelected = void (*)(void *caller, Location *loc);
using onDestinationSelectCancelled = void (*)(void *caller);

class Orrery
{
    // Positions come straight from Location::resolvedPosition() -- a parent-chain
    // walk of <= 3 hops.
    //
    // This replaced a body_positions[64] cache that was deliberate, not accidental:
    // it skipped all position work while time was paused, and made hit-testing an
    // array read. It was dropped because the fixed 64 entries capped a system's body
    // count and were written unchecked, which blocks facilities becoming locations.
    // The trade: while time advances this is no worse (the draw loop now resolves
    // once per body instead of reading a cached value twice); while time is PAUSED it
    // does per-frame work the cache avoided entirely. ~38 bodies x <=3 adds for Sol,
    // so trivial today -- but see todo.md if the orrery ever gets slow.
    inline Vector2 renderPosition(const Location *location)
    {
        // apply scale and center to the raw body position
        // adjust by focus in system coordinates to allow for zooming in on a particular body (e.g. planet)

        const Vector2 p = location->resolvedPosition();
        return {
            this->center.x + (p.x - focus.x) * this->scale,
            this->center.y + (p.y - focus.y) * this->scale};
    }

    Location *mouseOverBody();

public:
    Vector2 center; // center of output render
    Location *focus_location;
    Vector2 focus; // point to focus on - zoom around this point rather than center (e.g. for zooming in on a planet). This is in solar coordinates.
    float scale;

    System *system;

    void *caller; // optional pointer to caller state, e.g. to allow callbacks to trigger actions on the caller
    onDestinationSelected onDestinationSelectedCallback;
    onDestinationSelectCancelled onDestinationSelectCancelledCallback;

    Orrery();

    ~Orrery();

    void input();
    void render();

    Orrery &setSystem(System *s)
    {
        this->system = s;
        return *this;
    }

    Orrery &focusOnLocation(Location *location);
};

typedef std::unique_ptr<Orrery> OrreryPtr;

OrreryPtr createOrrery(Vector2 center, float scale);