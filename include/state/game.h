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
#include "state/research_topic.h"
#include "state/faction.h"
// Game state. Can be initialised, saved, loaded.
// Singleton for the moment.

const int MAX_SUPPLY_POD_AMOUNT = 250;
const int MAX_DRONE_FLEET_SIZE = 200;
const int BANDAID_REPAIR_RATE = 5; // how many damage points repaired per second of work
const int MAX_ORBITAL_STORAGE = 50000;

class Loader;

class ResearchFacility;

// abstracted, so we can test different simulations, or provide static values for testing
class TransitTimeCalculator
{
public:
    virtual float calculateTransitTime(Location *from, Location *to) = 0;
};

class LinearTransitTimeCalculator : public TransitTimeCalculator
{
public:
    float calculateTransitTime(Location *from, Location *to) override;
};

class EventSink;

class Game
{
    // owning collection of systems
    Systems systems;

    // owning collection of locations - system references. All locations to have unique ID for persistence.
    Locations locations;

    // owning collections of facilities
    Bases bases;
    Orbitals orbitals;

    // owning collection of IOS - maybe per-system?
    IOSs ios;

    // non-owning collection of factories
    std::vector<Factory *> factories;

    // non-owning collection of shuttles
    std::vector<Shuttle *> shuttles;

    // non-owning collection of research facilities
    std::vector<ResearchFacility *> researchFacilities;

    // current game instance
    static std::unique_ptr<Game> current;

public:
    // game state
    float game_time;
    float time_rate;

    // name counters
    int ios_number{1};
    int scg_number{1};
    int craft_max_id{0};

    // item definitions - array of instances as not passed around
    std::vector<Item> items;

    // research topics - array of instances as not passed around
    std::vector<ResearchTopic> researchTopics;

    // factions
    std::vector<Faction> factions;

    std::vector<EventSink *> eventSinks; // non-owning collection of event sinks to send game events to, e.g. for logging or triggering UI updates

    std::unique_ptr<TransitTimeCalculator> transitTimeCalculator;

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
    const std::vector<Shuttle *> &allShuttles() const { return shuttles; }

    EarthCity *createEarthCity(Location *location);
    ResourceFacility *createResourceFacility(Location *location);
    Orbital *createOrbital(Location *location);
    ResearchFacility *createResearchFacility(ResourceFacility *facility);

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
    Location *createLocation(System *system, const int id, const char *name, LocationType type);
    Factory *createFactory(Facility *facility);
    Shuttle *createShuttle(Location *location); // create at location - may not have a facility. Used by save/load
    Shuttle *createShuttle(Facility *facility);
    IOS *createIOS(Location *location); // create at location - may not have a facility. Used by save/load
    IOS *createIOS(Facility *facility);
    ResearchFacility *createResearchFacility(Facility *facility);

    // locations
    Locations &allLocations() { return locations; }
    Location *locationByID(int id);

    // requirements checks
    bool canCommissionShuttle(Facility *facility) const;
    bool canCommissionIOS(Facility *facility) const;

    // commission actions
    Shuttle *commissionShuttle(Facility *facility);
    IOS *commissionIOS(Facility *facility);

    // craft actions
    void setPodType(Craft *craft, int index, PodType pt, Facility *facility);
    void setSupplyPodContent(Pod *pod, Stores *stores, int resource_id, int amount);
    void setToolPodContent(Pod *pod, Stores *stores, int item_id);
    void unloadAllPods(Craft *craft, Facility *facility);
    bool loadWeapon(Craft *craft, int item_id, Facility *facility);
    bool canActivatePod(Craft *craft, int pod_index);
    bool activatePod(Craft *craft, int pod_index);
    bool updateActivePod(Craft *craft, Pod &pod, float delta); // returns true if pod still active after update, false if completed

    // weapon functions
    ItemType droneTypeForCraft(const Craft *craft) const;
    int droneCountForCraft(const Craft *craft) const;

    // update by delta
    void update(float delta);
    void advanceTick();

    // events
    void addEventSink(EventSink *sink);
    void removeEventSink(EventSink *sink);

    void raiseLogEvent(const char *log_text);
    void raiseOrbitalConstructionEvent(Orbital *orbital);
    void raiseResourceFacilityConstructionEvent(ResourceFacility *rf);
    void raiseProductionCompleteEvent(Factory *factory, int item_id);

    void onSpacecraftArrival(Craft *craft);

    // console input
    bool processConsoleCommand(const char *command, Location *l, Facility *f);
};