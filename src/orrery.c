#include "orrery.h"
#include <stdlib.h>
#include <math.h>

Orrery* createOrrery(Vector2 center, float scale)
{
    Orrery* orrery = malloc(sizeof(Orrery));
    orrery->center = center;
    orrery->scale = scale;
    orrery->system = NULL;

    return orrery;
}

void destroyOrrery(Orrery* orrery)
{    
    free(orrery);
}

void setSystem(Orrery* orrery, System* system)
{
    orrery->system = system;
}

void renderOrrery(Orrery* orrery)
{
    if (orrery->system == NULL) return;
    System* system = orrery->system;
    // batch lines
    for (int i = 0; i < system->numPlanets; i++)
    {
        DrawCircleLinesV(orrery->center, system->planetDistances[i] * orrery->scale, (Color){60, 60, 60, 255});
    }

    for (int i = 0; i < system->numPlanets; i++)
    {        
        Vector2 planet_pos = {
            orrery->center.x + system->planetPositions[i].x * orrery->scale,
            orrery->center.y + system->planetPositions[i].y * orrery->scale
        };        
        DrawCircleV(planet_pos, system->planetSizes[i] * orrery->scale, system->planetColors[i]);
    }
}