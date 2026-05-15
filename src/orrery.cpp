#include <cstdlib>
#include <cmath>

#include "orrery.h"
#include "state/game.h"
#include "pages/overlay.h"

const int MAX_BODY_COUNT = 64;

Orrery::Orrery() : last_time(-1.0f), focus_index(-1), system(nullptr), focus({0.0f, 0.0f})
{
    // constructor can be used to initialise any state the orrery needs to track
    // e.g. cached body positions, etc.
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

int Orrery::mouseOverBodyIndex()
{
    Vector2 mousePos = GetMousePosition();
    for (int i = 0; i < system->numPlanets; i++)
    {
        float planet_radius = system->planetSizes[i] * this->scale;
        if (CheckCollisionPointCircle(mousePos, renderPosition(i), planet_radius))
        {
            return i;
        }
    }
    return -1;
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
        int body_index = mouseOverBodyIndex();
        if (body_index != -1)
        {
            focus_index = body_index;
            focus.x = body_positions[body_index].x;
            focus.y = body_positions[body_index].y;
            TraceLog(LOG_INFO, "Focused on body: %s", system->locations[body_index]->name);
        }
        else
        {
            focus_index = -1;
            focus = {0.0f, 0.0f};
            TraceLog(LOG_INFO, "Focus cleared");
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        int body_index = mouseOverBodyIndex();
        if (body_index != -1)
        {
            TraceLog(LOG_INFO, "Clicked on body: %s", system->locations[body_index]->name);
            if (onDestinationSelectedCallback)
            {
                onDestinationSelectedCallback(caller, system->locations[body_index].get());
            }
        }
    }
}

void Orrery::render()
{
    if (this->system == NULL)
        return;

    // indicate focus
    if (focus_index != -1)
    {
        DrawText(system->locations[focus_index]->name, 980, 30, 20, GRAY);
    }

    // calculate body positions, cache then draw.
    // cache is for other, later indicators e.g. transit lines
    auto game{Game::getCurrent()};
    auto &overlay{Overlay::getInstance()};

    updatePositions(game->game_time);

    // batch lines for orbital radius
    for (int i = 0; i < system->numPlanets; i++)
    {
        int parent_id = system->planetPrimaryIndexes[i];
        if (parent_id == 0)
        {
            // offset center by focus
            Vector2 offset = {this->center.x - this->focus.x * this->scale, this->center.y - this->focus.y * this->scale};
            DrawCircleLinesV(offset, system->planetDistances[i] * this->scale, (Color){60, 60, 60, 255});
        }
    }

    Vector2 mousePos = GetMousePosition();

    for (int i = 0; i < system->numPlanets; i++)
    {
        float planet_radius = system->planetSizes[i] * this->scale;
        DrawCircleV(renderPosition(i), planet_radius, system->planetColors[i]);

        // if mouse is hovering over this body, show name
        if (CheckCollisionPointCircle(mousePos, renderPosition(i), planet_radius))
        {
            overlay.setCurrentToolTip(system->locations[i]->name);
        }
    }

    for (auto &craft : game->allIOS())
    {
        // need to find location index from pointer
        if (craft->state == CS_TRANSIT)
        {
            // determine source and destination locations from craft endpoints
            auto &current_dest{craft->currentDestination()};
            auto &prior_dest{craft->priorDestination()};

            if (current_dest.location && prior_dest.location)
            {
                int source_index = prior_dest.location->index;
                int dest_index = current_dest.location->index;

                Vector2 source_pos = renderPosition(source_index);
                Vector2 dest_pos = renderPosition(dest_index);

                // draw a line between source and destination
                DrawLineV(source_pos, dest_pos, (Color){64, 255, 192, 192});

                // interpolate position by state timer
                float progress = 1.0f - craft->state_timer / craft->total_state_timer;
                Vector2 pos = {
                    source_pos.x + (dest_pos.x - source_pos.x) * progress,
                    source_pos.y + (dest_pos.y - source_pos.y) * progress};

                DrawCircleV(pos, 5.0f, (Color){255, 255, 255, 255});
            }
        }
    }
}

void Orrery::updatePositions(float time)
{
    if (time == last_time)
        return; // no need to update
    last_time = time;
    // update body_positions based on current game time and system data
    // this is called before rendering to ensure positions are up to date
    // assumes parents are ordered before children in system->locations

    // positions DO NOT include rendering scale

    for (int i = 0; i < system->numPlanets; i++)
    {
        int parent_id = system->planetPrimaryIndexes[i];
        Vector2 parent_pos = {0.0f, 0.0f};
        if (parent_id > 0)
        {
            parent_pos.x = system->planetPositions[parent_id].x;
            parent_pos.y = system->planetPositions[parent_id].y;
        }
        Vector2 planet_pos = {
            (parent_pos.x + system->planetPositions[i].x),
            (parent_pos.y + system->planetPositions[i].y)};
        body_positions[i] = planet_pos;
    }
}

Orrery &Orrery::focusOnLocation(Location *location)
{
    if (location)
    {
        focus.x = body_positions[location->index].x;
        focus.y = body_positions[location->index].y;
        TraceLog(LOG_INFO, "Focused on location: %s", location->name);
    }
    else
    {
        focus = {0.0f, 0.0f};
        TraceLog(LOG_INFO, "Focus cleared");
    }
    return *this;
}