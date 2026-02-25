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
    int* planetPrimaryIndexes; // array index of primary body for each planet, -1 if primary (e.g. star)

} System;

System* createSystem(const bool asSolSystem);
void destroySystem(System* system);
void updateSystem(System* system, float time);