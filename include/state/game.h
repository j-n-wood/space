#pragma once

#include <cstdint>
#include <memory>
#include "state/system.h"
#include "state/resourceFacility.h"

// Game state. Can be initialised, saved, loaded.
// Singleton for the moment.

const int GAME_MAX_SYSTEMS = 16;

class Loader;

class Game
{

    // TODO this state is more UI
    System *currentSystem;
    Location *currentLocation;
    Facility *currentFacility;

    // game state
    uint32_t game_time;

    SystemPtr systems[GAME_MAX_SYSTEMS];

    // owning collections of facilities
    // Facilities orbitals;
    Bases bases;

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
    inline Facility *getCurrentFacility() const { return currentFacility; }
    inline Game &setCurrentFacility(Facility *f)
    {
        currentFacility = f;
        return *this;
    }

    // Singleton accessor
    static Game &getInstance()
    {
        static Game instance;
        return instance;
    }

    // add game state
    ResourceFacility *createResourceFacility(Location *location);

    // locate game state
    ResourceFacility *resourceFacilityAt(Location *location);

    // update by one tick
    void update();
};