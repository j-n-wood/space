#include <cstdlib>
#include <cmath>
#include <algorithm>

#include "orrery.h"
#include "state/game.h"
#include "pages/overlay.h"

Orrery::Orrery() : focus_location(nullptr), system(nullptr), focus({0.0f, 0.0f}), onDestinationSelectedCallback(nullptr), onDestinationSelectCancelledCallback(nullptr)
{
}

Orrery::~Orrery()
{
    // Note: we don't own the system pointer, so we won't free it here
}

OrreryPtr createOrrery(Vector2 center, float scale)
{
    Orrery *orrery = new Orrery();
    orrery->center = center;
    orrery->scale = scale;

    return OrreryPtr(orrery);
}

namespace
{
    // Floor for the band's hit half-width, in screen pixels. Zoomed out, the drawn band is a
    // pixel or two wide and would be unhittable.
    const float MIN_BAND_HIT_HALF_WIDTH = 6.0f;
}

void Orrery::rebuild()
{
    rendered.clear();
    if (!system)
    {
        return;
    }

    rendered.reserve(system->locations.size());
    for (Location *loc : system->locations)
    {
        LocationRender r;
        r.location = loc;
        r.screen_pos = renderPosition(loc);
        r.screen_radius = loc->radius * this->scale;

        if (loc->type == LOCATION_TYPE_ASTEROID_BELT)
        {
            // A belt is an annulus about the PRIMARY, not about its own position -- its
            // position is one point on the ring. Recorded here so the draw and the hit
            // test use the same two radii rather than each deriving them.
            //
            // A belt reads its two orbital elements differently from a body: orbital_radius
            // is the middle of the ring, and `radius` is the ring's half-width rather than a
            // display radius. So a belt has no disc -- drawing one would put a blob at the
            // single point on the ring the belt's position names.
            const float mid = loc->orbital_radius * this->scale;
            const float half = loc->radius * this->scale;
            r.band_inner = mid - half;
            r.band_outer = mid + half;
            r.screen_radius = 0.0f;
        }

        rendered.push_back(r);
    }
}

void Orrery::update()
{
    // Positions only move when time advances, so a paused orrery reuses the table it has.
    // The view transform is the other input, and zoom and pan invalidate by resetting the stamp rather than by stamping something of their own.
    Game *game = Game::getCurrent();
    if (game->game_time > last_update_time)
    {
        rebuild();
        last_update_time = game->game_time;
    }
}

const LocationRender *Orrery::rowFor(const Location *location) const
{
    for (const LocationRender &r : rendered)
    {
        if (r.location == location)
        {
            return &r;
        }
    }
    return nullptr;
}

Location *Orrery::locationAt(Vector2 point) const
{
    // Discs first, band second. A belt's band is wide enough to enclose the orbits of
    // planets inside it, so returning the band on the first match would make those
    // planets unclickable. A band hit is remembered and returned only if nothing else
    // matched -- it is the lowest-precedence target.
    const Vector2 centre = focusOffset();
    Location *band = nullptr;

    for (const LocationRender &r : rendered)
    {
        // radius 0 means not drawn, so not hit-testable either
        if (r.screen_radius > 0.0f && CheckCollisionPointCircle(point, r.screen_pos, r.screen_radius))
        {
            return r.location;
        }

        if (!band && r.band_outer > 0.0f)
        {
            // Annulus about the primary. Squared throughout: no sqrt needed to compare,
            // and this runs per location per frame for the tooltip.
            const float dx = point.x - centre.x;
            const float dy = point.y - centre.y;
            const float d2 = dx * dx + dy * dy;

            // Floor the hit width in screen pixels: zoomed out, the drawn band is a pixel
            // or two and would be unhittable. Generous is harmless -- it only wins when
            // nothing else did.
            const float mid = (r.band_inner + r.band_outer) * 0.5f;
            const float half = std::max((r.band_outer - r.band_inner) * 0.5f, MIN_BAND_HIT_HALF_WIDTH);
            const float inner = mid - half;
            const float outer = mid + half;

            if (d2 >= inner * inner && d2 <= outer * outer)
            {
                band = r.location;
            }
        }
    }

    return band;
}

Location *Orrery::mouseOverBody()
{
    return locationAt(GetMousePosition());
}

void Orrery::input()
{
    // handle input for the orrery, e.g. zoom, pan, selecting bodies, etc.
    // this is separate from render as it may be called at different times (e.g. when the orrery is not visible)
    Vector2 mouseWheel = GetMouseWheelMoveV();
    if (mouseWheel.y != 0)
    {
        this->scale += mouseWheel.y * 0.1f;
        if (this->scale < 0.1f)
            this->scale = 0.1f;

        // The table was built for the old transform. Zoom works while paused, so waiting for
        // time to advance would leave the orrery drawn at the previous scale.
        last_update_time = -1.0f;
    }

    // set focus to mouse position on right click, if over a body -- nullptr clears it, which
    // focusOnLocation already handles, so this is the same path the callers use
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        focusOnLocation(mouseOverBody());
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        Location *body = mouseOverBody();
        if (body)
        {
            TraceLog(LOG_INFO, "Clicked on body: %s", body->name);
            if (onDestinationSelectedCallback)
            {
                onDestinationSelectedCallback(caller, body);
            }
        }
    }
}

void Orrery::render()
{
    if (this->system == NULL)
        return;

    // indicate focus
    if (focus_location)
    {
        DrawText(focus_location->name, 980, 30, 20, GRAY);
    }

    auto game{Game::getCurrent()};
    auto &overlay{Overlay::getInstance()};

    // Orbit guides are drawn about the primary, so they share one offset centre.
    const Vector2 offset = focusOffset();

    for (const LocationRender &r : rendered)
    {
        if (r.location->type == LOCATION_TYPE_PLANET)
        {
            DrawCircleLinesV(offset, r.location->orbital_radius * this->scale, (Color){60, 60, 60, 255});
        }
        else if (r.band_outer > 0.0f)
        {
            // A belt has width where a planet has a line. Same two radii the hit test
            // uses, so what is clicked is what was drawn.
            DrawRing(offset, r.band_inner, r.band_outer, 0.0f, 360.0f, 64,
                     (Color){70, 60, 45, 90});
        }
    }

    for (const LocationRender &r : rendered)
    {
        DrawCircleV(r.screen_pos, r.screen_radius, r.location->color);
    }

    // The tooltip asks the same question a click does, rather than testing discs inline as
    // it used to -- the two answers could then differ, and for the belt band they would.
    if (Location *hovered = locationAt(GetMousePosition()))
    {
        overlay.setCurrentToolTip(hovered->name);
    }

    for (auto &craft : game->allIOS())
    {
        if (craft->inTransit())
        {
            // determine source and destination locations from craft endpoints
            auto &current_dest{craft->currentDestination()};
            auto &prior_dest{craft->priorDestination()};

            // Endpoints may be in another system, or be nothing at all. rowFor answers both
            // at once: no row means not drawn here. Taking the endpoints from the table
            // rather than resolving them again is what keeps the route line landing on the
            // drawn disc rather than near it.
            const LocationRender *source = rowFor(prior_dest.location);
            const LocationRender *dest = rowFor(current_dest.location);

            if (source && dest)
            {
                const Vector2 source_pos = source->screen_pos;
                const Vector2 dest_pos = dest->screen_pos;

                // draw a line between source and destination
                DrawLineV(source_pos, dest_pos, (Color){64, 255, 192, 192});

                // interpolate position by state timer
                float progress = craft->stateProgress();
                Vector2 pos = {
                    source_pos.x + (dest_pos.x - source_pos.x) * progress,
                    source_pos.y + (dest_pos.y - source_pos.y) * progress};

                DrawCircleV(pos, 5.0f, (Color){255, 255, 255, 255});
            }
        }
    }
}

Orrery &Orrery::focusOnLocation(Location *location)
{
    if (location)
    {
        const Vector2 p = location->resolvedPosition();
        focus.x = p.x;
        focus.y = p.y;
        focus_location = location;
        TraceLog(LOG_INFO, "Focused on location: %s", location->name);
    }
    else
    {
        focus = {0.0f, 0.0f};
        focus_location = nullptr;
        TraceLog(LOG_INFO, "Focus cleared");
    }

    // Moving the view invalidates every screen position in the table. Same marker the zoom
    // path uses, so there is one way for the table to go stale.
    last_update_time = -1.0f;

    return *this;
}