#include "system.h"
#include <stdlib.h>
#include <math.h>

System* createSystem(const bool asSolSystem)
{
    System* system = malloc(sizeof(System));

    system->numPlanets = 8;    

    if (!asSolSystem) {
        system->planetColors = NULL;
        system->planetDistances = NULL;
        system->planetSizes = NULL;
        system->planetVelocities = NULL;
        system->planetPositions = NULL;
        system->planetPrimaryIndexes = NULL;
        return system;
    } 

    system->planetDistances = malloc(sizeof(float) * system->numPlanets);
    system->planetDistances[0] = 80;
    system->planetDistances[1] = 110;
    system->planetDistances[2] = 165;
    system->planetDistances[3] = 225;
    system->planetDistances[4] = 310;
    system->planetDistances[5] = 430;
    system->planetDistances[6] = 515;
    system->planetDistances[7] = 565;

    // planet_velocities = {1.607f, 1.174f, 1.f, 0.802f, 0.434f, 0.323f, 0.228f, 0.182f};
    system->planetSizes = malloc(sizeof(float) * system->numPlanets);
    system->planetSizes[0] = 10;
    system->planetSizes[1] = 15;
    system->planetSizes[2] = 20;
    system->planetSizes[3] = 18;
    system->planetSizes[4] = 60;
    system->planetSizes[5] = 55;
    system->planetSizes[6] = 25;
    system->planetSizes[7] = 22;

    system->planetColors = malloc(sizeof(Color) * system->numPlanets);
    system->planetColors[0] = (Color){115, 147, 179, 255}; // Mercury
    system->planetColors[1] = (Color){255, 87, 51, 255}; // Venus
    system->planetColors[2] = (Color){30, 144, 255, 255}; // Earth
    system->planetColors[3] = (Color){178, 34, 34, 255}; // Mars
    system->planetColors[4] = (Color){210, 105, 30, 255}; // Jupiter
    system->planetColors[5] = (Color){220, 20, 60, 255};   // Saturn
    system->planetColors[6] = (Color){72, 209, 204, 255};  // Uranus
    system->planetColors[7] = (Color){65, 105, 225, 255};   // Neptune

    system->planetVelocities = malloc(sizeof(float) * system->numPlanets);
    system->planetVelocities[0] = 1.607f;
    system->planetVelocities[1] = 1.174f;
    system->planetVelocities[2] = 1.f;
    system->planetVelocities[3] = 0.802f;
    system->planetVelocities[4] = 0.434f;
    system->planetVelocities[5] = 0.323f;
    system->planetVelocities[6] = 0.228f;
    system->planetVelocities[7] = 0.182f;

    system->planetPositions = malloc(sizeof(Vector2) * system->numPlanets);
    for (int i = 0; i < system->numPlanets; i++)
    {        
        system->planetPositions[i] = (Vector2){system->planetDistances[i], 0};
    }

    system->planetPrimaryIndexes = malloc(sizeof(int) * system->numPlanets);
    for (int i = 0; i < system->numPlanets; i++)
    {
        system->planetPrimaryIndexes[i] = -1; // all planets orbit the star in this simple sol system
    }

    return system;
}

void destroySystem(System* system)
{
    if (system == NULL) return;
    free(system->planetDistances);
    free(system->planetSizes);
    free(system->planetColors);
    free(system->planetVelocities);
    free(system->planetPositions);
    free(system->planetPrimaryIndexes);
    free(system);
}

void updateSystem(System* system, float time)
{
for (int i = 0; i < system->numPlanets; i++)
    {        
        float angle = time * system->planetVelocities[i];
        system->planetPositions[i] = (Vector2){
            system->planetDistances[i] * cosf(angle),
            system->planetDistances[i] * sinf(angle)
        };
    }
}