#include "orrery.h"
#include <cstdlib>
#include <cmath>

OrreryPtr createOrrery(Vector2 center, float scale)
{
    Orrery* orrery = new Orrery();
    orrery->center = center;
    orrery->scale = scale;
    orrery->system = NULL;

    return OrreryPtr(orrery);
}

void Orrery::render()
{
    if (this->  system == NULL) return;
    System* system = this->system;
    // batch lines
    for (int i = 0; i < system->numPlanets; i++)
    {
        DrawCircleLinesV(this->center, system->planetDistances[i] * this->scale, (Color){60, 60, 60, 255});
    }

    for (int i = 0; i < system->numPlanets; i++)
    {
        int parent_id = system->planetPrimaryIndexes[i];
        Vector2 parent_pos = { 0.0f, 0.0f };
        if (parent_id > 0)
        {
            parent_pos.x = system->planetPositions[parent_id].x;
            parent_pos.y = system->planetPositions[parent_id].y;
        }
        Vector2 planet_pos = {
            this->center.x + ( parent_pos.x + system->planetPositions[i].x ) * this->scale,
            this->center.y + ( parent_pos.y + system->planetPositions[i].y ) * this->scale
        };
        DrawCircleV(planet_pos, system->planetSizes[i] * this->scale, system->planetColors[i]);
    }
}