#pragma once

#include "raylib.h"

typedef struct 
{
    Vector2 center;
    float scale;

    int numPlanets;
    float* planetDistances;
    float* planetSizes;
    Color* planetColors;
    float* planetVelocities;

} Orrery;

Orrery* createOrrery(Vector2 center, float scale);

void renderOrrery(Orrery* orrery, float time);

void destroyOrrery(Orrery* orrery);