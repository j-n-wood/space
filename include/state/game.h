#pragma once

#include <cstdint>
#include <memory>
#include "state/system.h"
#include "state/resourceFacility.h"
#include "state/orbital.h"
#include "state/factory.h"

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

    SystemPtr systems[GAME_MAX_SYSTEMS];

    // owning collections of facilities

    Bases bases;
    Orbitals orbitals;

    // non-owning collection of factories
    std::vector<Factory *> factories;

public:
    // game state
    uint32_t game_time;

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
    Orbital *createOrbital(Location *location);

    // locate game state
    ResourceFacility *resourceFacilityAt(Location *location);
    Orbital *orbitalAt(Location *location);

    // update by one tick
    void update();
};