#include <cstdlib>
#include <cmath>

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

Location *Orrery::mouseOverBody()
{
    Vector2 mousePos = GetMousePosition();
    for (Location *loc : system->locations)
    {
        // radius 0 means not drawn, so not hit-testable either
        float planet_radius = loc->radius * this->scale;
        if (CheckCollisionPointCircle(mousePos, renderPosition(loc), planet_radius))
        {
            return loc;
        }
    }
    return nullptr;
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
    }

    // set focus to mouse position on right click, if over a body
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        Location *body = mouseOverBody();
        if (body)
        {
            focus_location = body;
            const Vector2 p = body->resolvedPosition();
            focus.x = p.x;
            focus.y = p.y;
            TraceLog(LOG_INFO, "Focused on body: %s", body->name);
        }
        else
        {
            focus_location = nullptr;
            focus = {0.0f, 0.0f};
            TraceLog(LOG_INFO, "Focus cleared");
        }
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

    // batch lines for orbital radius
    for (Location *loc : system->locations)
    {
        if (loc->type == LOCATION_TYPE_PLANET)
        {
            // offset center by focus
            Vector2 offset = {this->center.x - this->focus.x * this->scale, this->center.y - this->focus.y * this->scale};
            DrawCircleLinesV(offset, loc->orbital_radius * this->scale, (Color){60, 60, 60, 255});
        }
    }

    Vector2 mousePos = GetMousePosition();

    for (Location *loc : system->locations)
    {
        float planet_radius = loc->radius * this->scale;
        Vector2 pos = renderPosition(loc);
        DrawCircleV(pos, planet_radius, loc->color);

        // if mouse is hovering over this body, show name
        if (CheckCollisionPointCircle(mousePos, pos, planet_radius))
        {
            overlay.setCurrentToolTip(loc->name);
        }
    }

    for (auto &craft : game->allIOS())
    {
        // need to find location index from pointer
        if (craft->inTransit() && craft->currentDestination().location && craft->priorDestination().location)
        {
            // determine source and destination locations from craft endpoints
            auto &current_dest{craft->currentDestination()};
            auto &prior_dest{craft->priorDestination()};

            // endpoints may be in another system; only draw the ones in this one
            if (current_dest.location && prior_dest.location &&
                current_dest.location->system == system && prior_dest.location->system == system)
            {
                Vector2 source_pos = renderPosition(prior_dest.location);
                Vector2 dest_pos = renderPosition(current_dest.location);

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
    return *this;
}