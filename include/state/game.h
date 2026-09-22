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
#include "state/object.h"
#include "state/resources.h"

// Game state. Can be initialised, saved, loaded.
// Singleton for the moment.

const int MAX_SUPPLY_POD_AMOUNT = 250;
const int MAX_DRONE_FLEET_SIZE = 200;
const int BANDAID_REPAIR_RATE = 5; // how many damage points repaired per second of work
const int MAX_ORBITAL_STORAGE = 50000;
const int REQUIRED_ORBITAL_FRAMES = 8;
const int REQUIRED_RESOURCE_FACILITY_FRAMES = 2;

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

    // owning collection of shuttles, as for IOS. Location::shuttle references into it.
    Shuttles shuttles;

    // non-owning collection of research facilities
    std::vector<ResearchFacility *> researchFacilities;

    // owning collection of objects
    uint32_t max_object_id{0}; // highest object id seen, so new objects get unique ids
    std::vector<std::unique_ptr<Object>> objects;

    // current game instance
    static std::unique_ptr<Game> current;

    // Take ownership of a location at slot `id`, keeping the id space dense. The one
    // place that writes `locations`, so every creation path -- bodies and facilities
    // alike -- lands where locationByID will look for it.
    Location *placeLocation(LocationPtr location, int id);

    // The orbit or surface location a facility of this kind belongs in. Accepts that
    // location directly, or a body, in which case the matching child is used. Bodies
    // and their regions come from the database; nothing creates a body at runtime, so
    // a missing region is an error rather than something to conjure.
    Location *facilityParentFor(Location *location, bool wantOrbit);

    // Shared setup for a facility created as a child location of `parent`: names it,
    // gives it a position relative to the parent, and links it into the system and
    // the parent's children.
    // `id` of -1 allocates the next free location id; the loader passes the
    // persisted one so identity survives a round trip.
    void attachFacilityLocation(Facility *facility, Location *parent, const char *label);

public:
    // game state
    float game_time;
    float time_rate;

    // name counters
    int ios_number{1};
    int scg_number{1};
    int craft_max_id{0};

    // Highest location id seen, so ids allocated at runtime continue the sequence
    // loaded from the database rather than colliding with it. Derived rather than
    // persisted -- a stored counter can drift from the data it describes, this
    // cannot. Maintained by createLocation, so every caller updates it. Starts at
    // -1 because body ids are 0-based.
    int location_max_id{-1};

    // Allocate the next free location id. Facilities become locations in due
    // course and draw their ids from here, extending the body sequence.
    inline int nextLocationID() { return ++location_max_id; }

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
    const Shuttles &allShuttles() const { return shuttles; }

    // `location` is the orbit or surface location the facility sits in -- that parent
    // is what makes it orbital or surface. A body may be given for convenience; it
    // resolves to the matching child.
    EarthCity *createEarthCity(Location *location, int id = -1);
    ResourceFacility *createResourceFacility(Location *location, int id = -1);
    Orbital *createOrbital(Location *location, int id = -1);
    ResearchFacility *createResearchFacility(ResourceFacility *facility);

    // locate game state
    ResourceFacility *resourceFacilityAt(Location *location);
    Orbital *orbitalAt(Location *location) const;

    // faction related
    void setFactionHostility(int faction_id, bool hostile);
    bool hostilesAt(Location *location, int faction_id);

    // logic to support UI
    inline bool locationHasShuttle(Location *location) const
    {
        return location && location->shuttle;
    }

    // create objects
    Location *createLocation(System *system, const int id, const char *name, LocationType type);
    Factory *createFactory(Facility *facility);
    // Create a shuttle AT a location. The owner is derived from location->body(), so
    // position and ownership cannot be confused. Sets no route: save/load restores the
    // recorded endpoints, and a manually built shuttle gets its route from
    // commissionShuttle. There is deliberately no Facility overload -- facility-specific
    // behaviour belongs to commissioning, not to creation.
    Shuttle *createShuttle(Location *location);

    // Give a shuttle the obvious route for a facility: this facility, and whatever is
    // on the other side of the same body. Only meaningful if both ends exist, which is
    // why it is a separate call rather than part of creation.
    void setDefaultRoute(Shuttle *shuttle, Facility *facility);

    // The precise location to aim a craft at, given somewhere vaguer. A facility is
    // already precise; a region resolves to the facility in it if there is one; a body
    // resolves through its orbit or surface region. This is what lets the destination
    // picker keep offering bodies while endpoints name exact places.
    Location *targetFor(Location *location, bool wantOrbit);
    // Create an IOS AT a location, which must be given: a craft is always somewhere, so
    // "nowhere in particular" is a system's space location rather than a null pointer.
    IOS *createIOS(Location *location);
    IOS *createIOS(Facility *facility); // at that facility's body
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
    bool updateCraftScanning(Craft *craft);                    // returns true scan target changed

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
    void onSpacecraftDocked(Craft *craft);
    void onSpacecraftLeaveBody(Craft *craft);
    void onCaptureOrbital(Orbital *orbital, int faction_id);
    void onWorkComplete(Craft *craft);
    void onWorkCancelled(Craft *craft);

    std::vector<std::unique_ptr<Object>> &allObjects() { return objects; }
    Object *createObject(int id, ObjectType type, Location *location, int quantity, int resource_id);
    Object *objectByID(int id);
    Object *randomiseAsteroid(Object *asteroid);
    void releaseScanTarget(Object *scan_object);

    // console input
    bool processConsoleCommand(const char *command, Location *l, Facility *f);
};