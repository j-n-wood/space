#pragma once

#include "raylib.h"

typedef struct 
{
    int numPlanets;
    float* planetDistances;
    float* planetSizes;
    Color* planetColors;
    float* planetVelocities;
    Vector2* planetPositions;

} System;

System* createSystem();
void destroySystem(System* system);
void updateSystem(System* system, float time);