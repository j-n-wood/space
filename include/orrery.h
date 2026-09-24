#pragma once

#include "state/system.h"
#include <memory>
#include <vector>

using onDestinationSelected = void (*)(void *caller, Location *loc);
using onDestinationSelectCancelled = void (*)(void *caller);

// One location as the orrery needs it: everything the draw, hover and click paths would
// otherwise each recompute. Screen space, so it belongs to one orrery -- the destination
// picker owns a second with its own scale and focus.
struct LocationRender
{
    Location *location{nullptr};
    Vector2 screen_pos{0.0f, 0.0f};
    float screen_radius{0.0f}; // 0 means not drawn, and so not hit-testable
    float band_inner{0.0f};    // asteroid belts only; 0 when this is not a band
    float band_outer{0.0f};
};

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

    // The screen point the primary sits at: renderPosition() of the system origin. Orbit
    // guides and the belt band are drawn about this, so the belt hit test must use it too
    // -- a belt's own position is a point ON its ring, not its centre.
    inline Vector2 focusOffset() const
    {
        return {this->center.x - focus.x * this->scale,
                this->center.y - focus.y * this->scale};
    }

    Location *mouseOverBody();

    // The row for a location, or nullptr if it is not in this orrery's system. A linear scan
    // over the system's locations, run only for craft actually in transit.
    const LocationRender *rowFor(const Location *location) const;

    // Built parallel to system->locations, so the index is just loop position. The old
    // body_positions[64] was indexed by a hand-maintained Location::index with a fixed
    // cap -- that is what made it unsafe, and what blocked facilities becoming locations.
    std::vector<LocationRender> rendered;

    // The game_time `rendered` was built for. -1 rather than 0 because a new game sits at
    // game_time 0 until time first advances, and a 0 stamp would never build a first table.
    double last_update_time{-1.0};

    void rebuild();

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

    // What is under a screen point, or nullptr. One answer for hover and click alike --
    // they used to be two copies of the same test, which is how ShuttleView's mouse and
    // keyboard paths drifted.
    Location *locationAt(Vector2 point) const;

    // Read-only view of the render table. Writing is rebuild()'s alone -- a row is only
    // meaningful alongside the scale and focus it was built for.
    inline const std::vector<LocationRender> &renderTable() const { return rendered; }

    void input();
    void render();

    // Rebuilds the render table when the positions or the view have changed. Called from
    // the owning page's update(), and once from its activate() so the first frame has a
    // table -- page update runs AFTER render in the frame.
    void update();

    Orrery &setSystem(System *s)
    {
        this->system = s;

        // A different system means different rows entirely, and time need not have advanced
        // for that to be true.
        last_update_time = -1.0;
        return *this;
    }

    Orrery &focusOnLocation(Location *location);
};

typedef std::unique_ptr<Orrery> OrreryPtr;

OrreryPtr createOrrery(Vector2 center, float scale);