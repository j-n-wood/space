#pragma once

#include "system.h"

typedef struct 
{
    Vector2 center;
    float scale;

    System* system;
} Orrery;

Orrery* createOrrery(Vector2 center, float scale);

void setSystem(Orrery* orrery, System* system);

void renderOrrery(Orrery* orrery);

void destroyOrrery(Orrery* orrery);