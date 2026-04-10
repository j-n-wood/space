#pragma once

#include <cstdint>
#include <memory>
#include "state/system.h"

// Game state. Can be initialised, saved, loaded.
// Singleton for the moment.

const int GAME_MAX_SYSTEMS = 16;

class Loader;

class Game
{
    SystemPtr systems[GAME_MAX_SYSTEMS];

    System *currentSystem;
    Location *currentLocation;

public:
    Game();
    ~Game();

    bool initialise(Loader *loader);

    System *getCurrentSystem() const { return currentSystem; }
    inline Location *getCurrentLocation() const { return currentLocation; }
    inline Game &setCurrentLocation(Location *l)
    {
        currentLocation = l;
        return *this;
    }

    // Singleton accessor
    static Game &getInstance()
    {
        static Game instance;
        return instance;
    }
};