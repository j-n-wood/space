#pragma once

#include <cstdint>
#include <memory>
#include "state/system.h"
#include "state/resourceFacility.h"
#include "state/earth_city.h"
#include "state/orbital.h"
#include "state/factory.h"
#include "state/item.h"
#include "state/shuttle.h"
#include "state/ios.h"

// Game state. Can be initialised, saved, loaded.
// Singleton for the moment.

const int MAX_SUPPLY_POD_AMOUNT = 250;

class Loader;

class Game
{
    // owning collection of systems
    Systems systems;

    // owning collections of facilities
    Bases bases;
    Orbitals orbitals;

    // owning collection of IOS - maybe per-system?
    IOSs ios;

    // non-owning collection of factories
    std::vector<Factory *> factories;

    // non-owning collection of shuttles
    std::vector<Shuttle *> shuttles;

    // current game instance
    static std::unique_ptr<Game> current;

public:
    // game state
    float game_time;
    float time_rate;

    // name counters
    int ios_number{1};
    int scg_number{1};

    // item definitions - array of instances as not passed around
    std::vector<Item> items;

    Game();
    ~Game();

    bool initialise(Loader *loader);

    // Singleton accessors
    static Game *getCurrent()
    {
        return current.get();
    }

    static Game *setCurrent(std::unique_ptr<Game> &newGame)
    {
        Game::current = std::move(newGame);
        return current.get();
    }

    static Game *createCurrent()
    {
        Game::current = std::make_unique<Game>();
        return current.get();
    }

    // add game state
    System *createSystem(int id, const char *name);
    const Systems &allSystems() const;
    const Bases &allBases() const;
    const Orbitals &allOrbitals() const;
    const IOSs &allIOS() const;

    EarthCity *createEarthCity(Location *location);
    ResourceFacility *createResourceFacility(Location *location);
    Orbital *createOrbital(Location *location);

    // locate game state
    ResourceFacility *resourceFacilityAt(Location *location);
    Orbital *orbitalAt(Location *location);
    Facility *facilityAt(const Endpoint &endpoint);

    // logic to support UI
    inline bool locationHasShuttle(Location *location) const
    {
        return location && location->shuttle;
    }

    // create objects
    Factory *createFactory(Facility *facility);
    Shuttle *createShuttle(Facility *facility);
    IOS *createIOS(Facility *facility);

    // requirements checks
    bool canCommissionShuttle(Facility *facility) const;
    bool canCommissionIOS(Facility *facility) const;

    // commission actions
    Shuttle *commissionShuttle(Facility *facility);
    IOS *commissionIOS(Facility *facility);

    void setPodType(Craft *craft, int index, PodType pt, Facility *facility);
    void setSupplyPodContent(Pod *pod, Stores *stores, int resource_id, int amount);
    void setToolPodContent(Pod *pod, Stores *stores, int item_id);

    // update by delta
    void update(float delta);
    void advanceTick();

    // events
    void onSpacecraftArrival(Craft *craft);
};