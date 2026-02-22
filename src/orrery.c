#include "orrery.h"
#include <stdlib.h>
#include <math.h>

Orrery* createOrrery(Vector2 center, float scale)
{
    Orrery* orrery = malloc(sizeof(Orrery));
    orrery->center = center;
    orrery->scale = scale;

    orrery->numPlanets = 8;    

    orrery->planetDistances = malloc(sizeof(float) * orrery->numPlanets);
    orrery->planetDistances[0] = 80;
    orrery->planetDistances[1] = 110;
    orrery->planetDistances[2] = 165;
    orrery->planetDistances[3] = 225;
    orrery->planetDistances[4] = 310;
    orrery->planetDistances[5] = 430;
    orrery->planetDistances[6] = 515;
    orrery->planetDistances[7] = 565;

    // planet_velocities = {1.607f, 1.174f, 1.f, 0.802f, 0.434f, 0.323f, 0.228f, 0.182f};
    orrery->planetSizes = malloc(sizeof(float) * orrery->numPlanets);
    orrery->planetSizes[0] = 10;
    orrery->planetSizes[1] = 15;
    orrery->planetSizes[2] = 20;
    orrery->planetSizes[3] = 18;
    orrery->planetSizes[4] = 60;
    orrery->planetSizes[5] = 55;
    orrery->planetSizes[6] = 25;
    orrery->planetSizes[7] = 22;

    orrery->planetColors = malloc(sizeof(Color) * orrery->numPlanets);
    orrery->planetColors[0] = (Color){115, 147, 179, 255}; // Mercury
    orrery->planetColors[1] = (Color){255, 87, 51, 255}; // Venus
    orrery->planetColors[2] = (Color){30, 144, 255, 255}; // Earth
    orrery->planetColors[3] = (Color){178, 34, 34, 255}; // Mars
    orrery->planetColors[4] = (Color){210, 105, 30, 255}; // Jupiter
    orrery->planetColors[5] = (Color){220, 20, 60, 255};   // Saturn
    orrery->planetColors[6] = (Color){72, 209, 204, 255};  // Uranus
    orrery->planetColors[7] = (Color){65, 105, 225, 255};   // Neptune

    orrery->planetVelocities = malloc(sizeof(float) * orrery->numPlanets);
    orrery->planetVelocities[0] = 1.607f;
    orrery->planetVelocities[1] = 1.174f;
    orrery->planetVelocities[2] = 1.f;
    orrery->planetVelocities[3] = 0.802f;
    orrery->planetVelocities[4] = 0.434f;
    orrery->planetVelocities[5] = 0.323f;
    orrery->planetVelocities[6] = 0.228f;
    orrery->planetVelocities[7] = 0.182f;

    return orrery;
}

void destroyOrrery(Orrery* orrery)
{
    free(orrery->planetDistances);
    free(orrery->planetSizes);
    free(orrery->planetColors);
    free(orrery);
}

void renderOrrery(Orrery* orrery, float time)
{
    // batch lines
    for (int i = 0; i < orrery->numPlanets; i++)
    {
        DrawCircleLinesV(orrery->center, orrery->planetDistances[i] * orrery->scale, (Color){60, 60, 60, 255});
    }

    for (int i = 0; i < orrery->numPlanets; i++)
    {        
        float angle = time * orrery->planetVelocities[i];
        Vector2 planet_pos = {
            orrery->center.x + cosf(angle) * orrery->planetDistances[i] * orrery->scale,
            orrery->center.y + sinf(angle) * orrery->planetDistances[i] * orrery->scale            
        };
        DrawCircleV(planet_pos, orrery->planetSizes[i] * orrery->scale, orrery->planetColors[i]);
    }
}