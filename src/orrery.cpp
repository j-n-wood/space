#include <cstdlib>
#include <cmath>

#include "orrery.h"
#include "state/game.h"

const int MAX_BODY_COUNT = 64;

OrreryPtr createOrrery(Vector2 center, float scale)
{
    Orrery *orrery = new Orrery();
    orrery->center = center;
    orrery->scale = scale;
    orrery->system = NULL;

    return OrreryPtr(orrery);
}

void Orrery::render()
{
    if (this->system == NULL)
        return;
    // batch lines
    for (int i = 0; i < system->numPlanets; i++)
    {
        DrawCircleLinesV(this->center, system->planetDistances[i] * this->scale, (Color){60, 60, 60, 255});
    }

    // calculate body positions, cache then draw.
    // cache is for other, later indicators e.g. transit lines

    Vector2 bodies[MAX_BODY_COUNT];
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
            this->center.x + (parent_pos.x + system->planetPositions[i].x) * this->scale,
            this->center.y + (parent_pos.y + system->planetPositions[i].y) * this->scale};
        bodies[i] = planet_pos;
        DrawCircleV(planet_pos, system->planetSizes[i] * this->scale, system->planetColors[i]);
    }

    auto game{Game::getCurrent()};

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

                Vector2 source_pos = bodies[source_index];
                Vector2 dest_pos = bodies[dest_index];

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