#pragma once

extern "C" {
    #include "raylib.h"
}

#include <memory>

class System
{
    public:
    int numPlanets;
    float* planetDistances;
    float* planetSizes;
    Color* planetColors;
    float* planetVelocities;
    Vector2* planetPositions;
    int* planetPrimaryIndexes; // array index of primary body for each planet, -1 if primary (e.g. star)

    ~System();

    void update(float time);
};

typedef std::unique_ptr<System> SystemPtr;

SystemPtr createSystem(const bool asSolSystem);
