#include "loaders/loader.h"
#include "state/game.h"
#include "state/research_facility.h"
#include "state/event_sink.h"
#include "state/craft_action.h"

#include <cstdio>
#include <cmath>
#include <algorithm> // std::remove - needed?

std::unique_ptr<Game> Game::current;

const float MAX_TIMESTEP = 1.0f;

// initial.db ships 173 bodies and 13 facilities; the orbit and surface locations take
// that to roughly 490. Reserving past it means createLocation's resize never has to
// reallocate while a game loads one location at a time. Growth beyond this is still
// correct, just not free.
const size_t INITIAL_LOCATION_CAPACITY = 512;

EventSink nullEventSink;

float LinearTransitTimeCalculator::calculateTransitTime(Location *from, Location *to)
{
    // simple implementation based on distance and fixed speed. Could be improved with more complex logic based on e.g. fuel efficiency, gravity assists, etc.
    if (!from || !to)
    {
        TraceLog(LOG_ERROR, "Null location provided to calculateTransitTime");
        return 0.0f;
    }
    auto sv2 = from->resolvedPosition();
    auto dv2 = to->resolvedPosition();
    float distance = sqrtf(powf(sv2.x - dv2.x, 2) + powf(sv2.y - dv2.y, 2));
    const float speed = 20.0f; // arbitrary speed factor to get reasonable transit times based on system scale

    return distance / speed;
}

Game::Game() : game_time(0.0f), time_rate(1.0f), transitTimeCalculator(std::make_unique<LinearTransitTimeCalculator>())
{
    locations.reserve(INITIAL_LOCATION_CAPACITY);
}

Game::~Game()
{
}

System *Game::createSystem(int id, const char *name)
{
    systems.emplace_back(std::make_unique<System>());
    auto sys = systems.back().get();
    sys->id = id;
    copyFixed(sys->name, sizeof sys->name, name);
    return sys;
}

const Systems &Game::allSystems() const
{
    return systems;
}

const Bases &Game::allBases() const
{
    return bases;
}

const Orbitals &Game::allOrbitals() const
{
    return orbitals;
}

const IOSs &Game::allIOS() const
{
    return ios;
}

namespace
{
    // Separation of a facility from the centre of its orbit or surface region: enough
    // to give siblings distinct positions for transit maths, small enough that reaching
    // one costs about the same as reaching another.
    const float FACILITY_SIBLING_RADIUS = 0.5f;

    // Spread siblings around the parent so several facilities at one body get
    // distinct positions. The golden angle avoids clustering as the count grows.
    float nextSiblingAngle(const Location *parent)
    {
        const float GOLDEN_ANGLE = 2.399963f; // ~137.5 degrees
        return static_cast<float>(parent->children.size()) * GOLDEN_ANGLE;
    }
}

Location *Game::placeLocation(LocationPtr location, int id)
{
    if (!location || id < 0)
    {
        TraceLog(LOG_ERROR, "placeLocation needs a location and a non-negative id (%d)", id);
        return nullptr;
    }

    // Placed BY id rather than appended, so ids stay dense whatever order things load
    // in and locationByID can index straight in. An unused id is a null slot.
    //
    // The constructor reserves past the expected count, so this does not reallocate in
    // practice. If it did, callers are safe: they hold Location*, which points at the
    // heap object rather than into this vector.
    if (static_cast<size_t>(id) >= locations.size())
    {
        locations.resize(static_cast<size_t>(id) + 1);
    }
    if (locations[id])
    {
        TraceLog(LOG_ERROR, "Duplicate location id %d (%s)", id, location->name);
        return nullptr;
    }

    location->id = id;
    locations[id] = std::move(location);

    if (location_max_id < id)
    {
        location_max_id = id;
    }
    return locations[id].get();
}

Location *Game::facilityParentFor(Location *location, bool wantOrbit)
{
    if (!location)
    {
        return nullptr;
    }
    // Already the right kind of region
    if (location->type == (wantOrbit ? LOCATION_TYPE_ORBIT : LOCATION_TYPE_SURFACE))
    {
        return location;
    }
    // A body was given: resolve to its orbit or surface child
    Location *b = location->body();
    if (!b)
    {
        return nullptr;
    }
    Location *parent = wantOrbit ? b->orbit() : b->surface();
    if (!parent && b->type != LOCATION_TYPE_SPACE)
    {
        // Every body in the database ships with its regions, and nothing creates a
        // body at runtime -- so this means the data is wrong, not that a region needs
        // making. A system's `space` is the exception: nothing orbits or lands on it.
        TraceLog(LOG_ERROR, "%s has no %s region", b->name, wantOrbit ? "orbit" : "surface");
    }
    return parent;
}

void Game::attachFacilityLocation(Facility *facility, Location *parent, const char *label)
{
    facility->system = parent->system;
    facility->primary = parent;
    facility->primary_id = parent->id;
    std::snprintf(facility->name, sizeof facility->name, "%s %s", parent->body()->name, label);

    // A facility sits within its orbit or surface region, so it needs only a small
    // offset -- enough to give siblings distinct positions for transit maths. The
    // standoff from the body itself belongs to the orbit location.
    facility->orbital_radius = FACILITY_SIBLING_RADIUS;
    facility->orbital_velocity = 0.0f; // holds station relative to the parent
    facility->initial_angle = nextSiblingAngle(parent);
    facility->position = (Vector2){
        facility->orbital_radius * cosf(facility->initial_angle),
        facility->orbital_radius * sinf(facility->initial_angle)};

    parent->children.push_back(facility);
    if (parent->system)
    {
        parent->system->locations.push_back(facility);
    }
}

EarthCity *Game::createEarthCity(Location *location, int id)
{
    Location *parent = facilityParentFor(location, false);
    if (!parent)
    {
        TraceLog(LOG_ERROR, "No surface location for createEarthCity");
        return nullptr;
    }
    // Game::locations owns it; bases below is a non-owning view.
    EarthCity *ec = static_cast<EarthCity *>(
        placeLocation(std::make_unique<EarthCity>(parent), (id >= 0) ? id : nextLocationID()));
    if (!ec)
    {
        return nullptr;
    }
    attachFacilityLocation(ec, parent, "City");
    bases.push_back(ec);

    auto factory = createFactory(ec); // EC production
    factory->is_orbital = false;      // EC is surface facility, so set factory accordingly
    factory->tech_level = 1;          // EC starts with tech level 1, can build basic items
    createResearchFacility(ec);
    return ec;
}

ResourceFacility *Game::createResourceFacility(Location *location, int id)
{
    Location *parent = facilityParentFor(location, false);
    if (!parent)
    {
        TraceLog(LOG_ERROR, "No surface location for createResourceFacility");
        return nullptr;
    }
    ResourceFacility *rf = static_cast<ResourceFacility *>(
        placeLocation(std::make_unique<ResourceFacility>(parent), (id >= 0) ? id : nextLocationID()));
    if (!rf)
    {
        return nullptr;
    }
    attachFacilityLocation(rf, parent, "Station");
    bases.push_back(rf);
    return rf;
}

ResourceFacility *Game::resourceFacilityAt(Location *location)
{
    if (location)
    {
        // Facilities hang off the body's surface location, not the body itself, so
        // match on the body. Accepts a body or either of its regions.
        Location *b = location->body();
        for (ResourceFacility *base : bases)
        {
            if (base->body() == b)
            {
                return base;
            }
        }
    }
    return nullptr;
}

Orbital *Game::createOrbital(Location *location, int id)
{
    Location *parent = facilityParentFor(location, true);
    if (!parent)
    {
        TraceLog(LOG_ERROR, "No orbit location for createOrbital");
        return nullptr;
    }
    Orbital *o = static_cast<Orbital *>(
        placeLocation(std::make_unique<Orbital>(parent), (id >= 0) ? id : nextLocationID()));
    if (!o)
    {
        return nullptr;
    }
    attachFacilityLocation(o, parent, "Orbital");
    orbitals.push_back(o);
    createFactory(o);
    return o;
}

Orbital *Game::orbitalAt(Location *location) const
{
    if (location)
    {
        // Facilities hang off the body's orbit location, not the body itself, so
        // match on the body. Accepts a body or either of its regions.
        const Location *b = location->body();
        for (Orbital *orbital : orbitals)
        {
            if (orbital->body() == b)
            {
                return orbital;
            }
        }
    }
    return nullptr;
}

Location *Game::createLocation(System *system, const int id, const char *name, LocationType type)
{
    Location *locPtr = placeLocation(std::make_unique<Location>(system, id, name, type), id);
    if (!locPtr)
    {
        return nullptr;
    }

    if (system)
    {
        system->locations.push_back(locPtr);
    }
    return locPtr;
}

Location *Game::locationByID(int id)
{
    // O(1), and correct by construction: placeLocation stores by id, so the slot
    // either holds that location or is empty.
    if (id < 0 || static_cast<size_t>(id) >= locations.size())
    {
        return nullptr;
    }
    return locations[id].get();
}

Factory *Game::createFactory(Facility *facility)
{
    Factory *f{facility->createFactory()};
    factories.push_back(f);
    return f;
}

ResearchFacility *Game::createResearchFacility(ResourceFacility *facility)
{
    if (!facility)
    {
        TraceLog(LOG_ERROR, "Null facility provided to createResearchFacility");
        return nullptr;
    }
    if (facility->research_facility)
    {
        TraceLog(LOG_ERROR, "Facility already has research facility");
        return nullptr;
    }
    facility->research_facility = std::make_unique<ResearchFacility>();
    ResearchFacility *rf = facility->research_facility.get();
    researchFacilities.push_back(rf);
    return rf;
}

bool Game::canCommissionShuttle(Facility *facility) const
{
    // require ios_chassis item, and no existing shuttle at the body. The shuttle hangs
    // off the body, not off this facility's orbit or surface region, so ask for the body.
    if (!facility || !facility->body())
    {
        return false;
    }
    if (facility->body()->shuttle)
    {
        return false;
    }
    return facility->stores.items[ItemType::S_Chassis] > 0;
}

bool Game::canCommissionIOS(Facility *facility) const
{
    if (!facility || !facility->body())
    {
        return false;
    }
    return facility->stores.items[ItemType::I_Chassis] > 0;
}

Shuttle *Game::commissionShuttle(Facility *facility)
{
    if (!canCommissionShuttle(facility))
    {
        return nullptr;
    }
    // remove required item from stores
    facility->stores.items[ItemType::S_Chassis] -= 1;
    auto shuttle = createShuttle(facility->body());
    if (!shuttle)
    {
        return nullptr;
    }
    setDefaultRoute(shuttle, facility);
    // assign drive if available
    if (facility->stores.items[ItemType::S_Drive] > 0)
    {
        facility->stores.items[ItemType::S_Drive] -= 1;
        shuttle->drive = true;
        shuttle->fuel = 250; // initial fuel for new shuttle, could be based on drive type or other factors
    }
    return shuttle;
}

IOS *Game::commissionIOS(Facility *facility)
{
    if (!canCommissionIOS(facility))
    {
        return nullptr;
    }
    // remove required item from stores
    facility->stores.items[ItemType::I_Chassis] -= 1;
    auto i = createIOS(facility);
    // assign drive if available
    if (facility->stores.items[ItemType::I_Drive] > 0)
    {
        facility->stores.items[ItemType::I_Drive] -= 1;
        i->drive = true;
        i->fuel = 250; // initial fuel for new IOS, could be based on drive type or other factors
    }
    return i;
}

// `position` is where the shuttle is; its OWNER is derived from position->body(), so the
// two cannot be confused. They coincide today, but once a docked craft's location is the
// facility, passing a facility here must still register the shuttle against its body --
// otherwise a save/load reparents it somewhere nothing looks.
Shuttle *Game::createShuttle(Location *position)
{
    if (!position)
    {
        TraceLog(LOG_ERROR, "Null location provided to createShuttle");
        return nullptr;
    }
    Location *home = position->body();
    if (!home)
    {
        TraceLog(LOG_ERROR, "No body for shuttle position %s", position->name);
        return nullptr;
    }
    if (home->shuttle)
    {
        TraceLog(LOG_ERROR, "Blocked createShuttle: %s already has one", home->name);
        return nullptr;
    }

    shuttles.emplace_back(std::make_unique<Shuttle>(CS_IDLE, 1, position));
    Shuttle *s = shuttles.back().get();
    s->id = ++craft_max_id;
    home->shuttle = s; // non-owning reference on the body
    // set default name
    std::snprintf(s->name, sizeof s->name, "%s Shuttle", home->name);
    return s;
}

Location *Game::targetFor(Location *location, bool wantOrbit)
{
    if (!location)
    {
        return nullptr;
    }
    if (location->isFacility())
    {
        return location; // already exact
    }

    Location *region = facilityParentFor(location, wantOrbit);
    if (!region)
    {
        return nullptr;
    }

    // Prefer the station in that region; the bare region is the answer when there is
    // none -- which is what "orbit Mars, no station" means.
    Facility *f = wantOrbit ? static_cast<Facility *>(orbitalAt(region))
                            : static_cast<Facility *>(resourceFacilityAt(region));
    return f ? static_cast<Location *>(f) : region;
}

void Game::setDefaultRoute(Shuttle *shuttle, Facility *facility)
{
    if (!shuttle || !facility || !facility->body())
    {
        TraceLog(LOG_ERROR, "setDefaultRoute needs a shuttle and a sited facility");
        return;
    }

    // Start docked at the facility we were commissioned from -- and located there, or
    // state and position would disagree from the outset.
    const bool hereIsOrbit = facility->inOrbit();
    shuttle->location = facility;

    // A shuttle runs between the two sides of one body, so the obvious route is this
    // facility and whatever is on the other side: a station if there is one, the bare
    // region otherwise. Only a sensible default, not a rule -- the far end may be empty.
    Location *b = facility->body();
    shuttle->destinations[0] = Endpoint(targetFor(b, !hereIsOrbit));
    shuttle->destinations[1] = Endpoint(facility);
}

IOS *Game::createIOS(Location *location)
{
    // A craft is always somewhere: "nowhere in particular" is a system's space location,
    // not a null pointer.
    if (!location)
    {
        TraceLog(LOG_ERROR, "Null location provided to createIOS");
        return nullptr;
    }

    ios.emplace_back(std::make_unique<IOS>(CS_IDLE, 3, location));
    auto i = ios.back().get();
    // Aim at the orbital if the body has one, otherwise its orbit region.
    Location *target = targetFor(location, true);
    i->destinations[0] = Endpoint(target);
    i->destinations[1] = Endpoint(target);
    // generate a name based on creation count
    std::snprintf(i->name, sizeof i->name, "IOS-%04d", ios_number++);
    i->id = ++craft_max_id;
    return i;
}

IOS *Game::createIOS(Facility *facility)
{
    // create an IOS at a location, starting at the given facility's body
    Location *location{facility ? facility->body() : nullptr};
    if (!location)
    {
        TraceLog(LOG_ERROR, "Facility missing location");
        return nullptr;
    }

    auto i = createIOS(location);

    return i;
}

// done here to combine game logic when pod type changes
void Game::setPodType(Craft *craft, int index, PodType pt, Facility *facility)
{
    if (!craft)
    {
        TraceLog(LOG_ERROR, "Missing craft to SetPodType");
        return;
    }
    if (index > craft->max_pods)
    {
        TraceLog(LOG_ERROR, "Invalid index to SetPodType");
        return;
    }

    // weapons are craft-wide: changing the type of any weapon pod unloads the whole craft first
    if (craft->pods[index].type == PT_WEAPON)
    {
        unloadAllPods(craft, facility);
    }

    // unload existing
    Pod &pod{craft->pods[index]};
    switch (pod.type)
    {
    case PT_TOOL:
        if (pod.amount)
        {
            facility->stores.items[pod.contentType] += pod.amount;
            pod.amount = 0;
        }
        break;
    case PT_SUPPLY:
        if (pod.amount)
        {
            facility->stores.resources[pod.contentType] += pod.amount;
            pod.amount = 0;
        }
        break;
        // TODO CRYO
    default:
        break;
    }

    // set new pod type
    pod.type = pt;
}

void Game::setSupplyPodContent(Pod *pod, Stores *stores, int resource_id, int amount)
{
    if (!pod || !stores)
    {
        TraceLog(LOG_ERROR, "Missing pod or stores to SetSupplyPodContent");
        return;
    }

    // remove existing content if any
    if (pod->amount > 0)
    {
        stores->resources[pod->contentType] += pod->amount;
        pod->amount = 0;
        pod->contentType = -1; // empty
    }

    if (resource_id >= 0)
    {
        // set new content
        pod->contentType = resource_id;
        // set to available amount in stores, or requested amount, whichever is lower. For now we assume supply pods have capacity of 250.
        pod->amount = std::min(stores->resources[resource_id], amount);
        // remove from stores
        stores->resources[resource_id] -= pod->amount;
    }
}

void Game::setToolPodContent(Pod *pod, Stores *stores, int item_id)
{
    if (!pod || !stores)
    {
        TraceLog(LOG_ERROR, "Missing pod or stores to SetToolPodContent");
        return;
    }

    if (items[item_id].pod_type != PT_TOOL)
    {
        TraceLog(LOG_ERROR, "Item %d is not a tool-pod item; use loadWeapon for weapons", item_id);
        return;
    }

    // remove existing content if any
    if (pod->amount > 0)
    {
        stores->items[pod->contentType] += pod->amount;
        pod->amount = 0;
    }

    auto pod_capacity = items[item_id].pod_capacity;

    if (pod_capacity <= 0)
    {
        TraceLog(LOG_ERROR, "Attempting to load item %d that is not loadable in pods", item_id);
        return;
    }

    if (item_id >= 0)
    {
        // set new content
        pod->contentType = item_id;
        // set to available amount in stores, or max pod capacity, whichever is lower.
        pod->amount = std::min(stores->items[item_id], pod_capacity);
        // remove from stores
        stores->items[item_id] -= pod->amount;
    }
}

void Game::unloadAllPods(Craft *craft, Facility *facility)
{
    if (!craft || !facility)
    {
        TraceLog(LOG_ERROR, "Missing craft or facility to unloadAllPods");
        return;
    }

    bool weaponReturned{false};
    for (int i = 0; i < craft->max_pods; ++i)
    {
        Pod &pod{craft->pods[i]};
        switch (pod.type)
        {
        case PT_TOOL:
            if (pod.amount)
            {
                facility->stores.items[pod.contentType] += pod.amount;
            }
            break;
        case PT_SUPPLY:
            if (pod.amount)
            {
                facility->stores.resources[pod.contentType] += pod.amount;
            }
            break;
        case PT_WEAPON:
            // weapon is craft-wide: one item back to stores, not one per pod
            if (!weaponReturned)
            {
                facility->stores.items[pod.contentType] += 1;
                weaponReturned = true;
            }
            break;
        case PT_CRYO:
            // TODO
            break;
        default:
            break;
        }
        pod.type = PT_EMPTY;
        pod.contentType = 0;
        pod.amount = 0;
    }
}

bool Game::loadWeapon(Craft *craft, int item_id, Facility *facility)
{
    if (!craft || !facility)
    {
        TraceLog(LOG_ERROR, "Missing craft or facility to loadWeapon");
        return false;
    }
    if (item_id < 0 || item_id >= (int)items.size())
    {
        TraceLog(LOG_ERROR, "Invalid item_id %d to loadWeapon", item_id);
        return false;
    }
    if (items[item_id].pod_type != PT_WEAPON)
    {
        TraceLog(LOG_ERROR, "Item %d is not a weapon", item_id);
        return false;
    }
    if (facility->stores.items[item_id] < 1)
    {
        return false;
    }

    unloadAllPods(craft, facility);

    for (int i = 0; i < craft->max_pods; ++i)
    {
        Pod &pod{craft->pods[i]};
        pod.type = PT_WEAPON;
        pod.contentType = item_id;
        pod.amount = 0; // number of drones, etc
    }

    facility->stores.items[item_id] -= 1;
    return true;
}

bool Game::canActivatePod(Craft *craft, int pod_index)
{
    if (!craft)
    {
        TraceLog(LOG_ERROR, "Missing craft to canActivatePod");
        return false;
    }
    if (pod_index > craft->max_pods)
    {
        TraceLog(LOG_ERROR, "Invalid index to canActivatePod");
        return false;
    }

    Pod &pod{craft->pods[pod_index]};

    // currently only tool pods can activate
    if (pod.type != PT_TOOL || pod.amount == 0)
    {
        return false;
    }
    // switch on content type of tool pod to check specific activation requirements
    const Item &item{items[pod.contentType]};

    if (!item.does_work)
    {
        return false;
    }

    switch (item.id)
    {
    case ItemType::Of_Frame:
        // must be in orbit, and no existing facility, or facility incomplete
        {
            Orbital *orbital = orbitalAt(craft->location);
            if (craft->inOrbit() && (!orbital || !orbital->operational))
            {
                return true;
            }
        }
        break;
    case ItemType::R_Frame:
        // must be on surface, not docked, no facility at location or facility incomplete
        {
            ResourceFacility *rf = resourceFacilityAt(craft->location);
            if (craft->location && !craft->inOrbit() && (!rf || !rf->operational))
            {
                return true;
            }
        }
        break;
    // grapple - must be in orbit, and have something to take/release
    // AMA - must be in orbit, at location of type asteroids
    // bandaid - must be docked on surface, and have damaged facility
    case ItemType::Bandaid:
        if (craft->location->type == LOCATION_TYPE_RESOURCE_FACILITY)
        {
            ResourceFacility *rf = resourceFacilityAt(craft->location);
            if (rf && rf->damage > 0)
            {
                return true;
            }
        }
        break;
    default:
        // some other thing
        break;
    }

    return false;
}

bool Game::activatePod(Craft *craft, int pod_index)
{
    // sanity check
    if (!canActivatePod(craft, pod_index))
    {
        TraceLog(LOG_ERROR, "Attempting to activate pod that cannot be activated: craft %s, pod index %d", craft->name, pod_index);
        return false;
    }

    Pod &pod{craft->pods[pod_index]};
    const Item &item{items[pod.contentType]};

    // mark pod as active and set craft state to working

    float work_time = item.work_parameters.work_time;
    if (pod.contentType == ItemType::Bandaid)
    {
        ResourceFacility *rf = resourceFacilityAt(craft->location);
        if (rf)
        {
            work_time = 1.0 + rf->damage / BANDAID_REPAIR_RATE; // time to repair is based on damage level
        }
    }

    craft->active_pod_index = pod_index;
    craft->setTimedState(CS_WORKING, work_time);

    return true;
}

bool Game::updateActivePod(Craft *craft, Pod &pod, float delta)
{
    if (!craft)
    {
        TraceLog(LOG_ERROR, "Missing craft to updateActivePod");
        return false;
    }

    // currently only tool pods can activate, so look for active tool pod

    if (pod.type == PT_TOOL && pod.amount > 0)
    {
        const Item &item{items[pod.contentType]};
        switch (item.id)
        {
        case ItemType::Bandaid:
            // repair based on time in state and repair rate, until fully repaired
            {
                ResourceFacility *rf = resourceFacilityAt(craft->location);
                if (rf)
                {
                    rf->damage = std::max(0.0f, rf->damage - (delta * BANDAID_REPAIR_RATE));
                    if (rf->damage == 0)
                    {
                        TraceLog(LOG_INFO, "Completed repairing facility at location %s", craft->location->name);
                        // set work timer to tiny s.t. update triggers work complete
                        craft->setTimedState(CS_WORKING, 0.001f);
                        return false;
                    }
                }
            }
            break;
        default:
            // nothing to update
            return false;
        }
    }

    return true; // still active
}

ItemType Game::droneTypeForCraft(const Craft *craft) const
{
    if (!craft)
    {
        TraceLog(LOG_ERROR, "Missing craft to droneTypeForCraft");
        return ItemType::None; // default to something non-weapon
    }

    if (craft->pods[0].contentType != ItemType::DFCC)
    {
        TraceLog(LOG_ERROR, "Craft has no DFCC in droneTypeForCraft");
        return ItemType::None; // default to something non-weapon
    }

    // for now we just return a single drone type based on craft type, but could be more complex in future with different drone types, or choice of drone type based on available items, etc.
    switch (craft->type)
    {
    case CraftType::CT_SCG:
        return ItemType::Star_Drone; // default to something non-weapon
    case CraftType::CT_IOS:
        return ItemType::Ios_Drone;
    default:
        TraceLog(LOG_ERROR, "Unknown craft type in droneTypeForCraft: %d", craft->type);
        return ItemType::None; // default to something non-weapon
    }
}

int Game::droneCountForCraft(const Craft *craft) const
{
    // use amount on pod 0
    if (!craft)
    {
        TraceLog(LOG_ERROR, "Missing craft to droneCountForCraft");
        return 0;
    }
    if (craft->pods[0].contentType != ItemType::DFCC)
    {
        TraceLog(LOG_ERROR, "Craft has no DFCC in droneCountForCraft");
        return 0;
    }
    return craft->pods[0].amount;
}

void Game::update(float delta)
{
    // add to time, if ticks over one second call advanceTick
    float dt = delta * time_rate;
    if (dt > MAX_TIMESTEP)
    {
        dt = MAX_TIMESTEP;
    }

    int prior{static_cast<int>(game_time)};
    game_time += dt;

    // start with updating all systems - could only update visible ones
    for (auto &system : systems)
    {
        system->update(game_time);
    }

    // update shuttles - use realtime as start of state changes is not tick aligned
    for (auto &shuttle : shuttles)
    {
        shuttle->update(dt);
    }

    for (auto &craft : ios)
    {
        craft->update(dt);
    }

    for (auto &rf : researchFacilities)
    {
        rf->update(dt);
    }

    int difference = static_cast<int>(game_time) - prior;
    while (difference > 0)
    {
        advanceTick();
        --difference;
    }
}

void Game::advanceTick()
{
    // update all facilities
    for (auto &base : bases)
    {
        base->update();
    }

    // update factories
    for (auto factory : factories)
    {
        factory->update();
    }
}

void Game::onSpacecraftArrival(Craft *craft)
{
    // craft->arrive() already called
    // this can trigger game events
}

void Game::onSpacecraftDocked(Craft *craft)
{
    // craft->onDocked() already called
    // this can trigger game events that do not relate to 'boarding' e.g. trade.

    // if docking location is hostile, trigger capture event
    if (hostilesAt(craft->location, craft->faction_id) && (craft->hasCapability(CC_BOARDING)))
    {
        // TODO: orbital self destruct
        // switch ownership of orbital to craft's faction
        Orbital *orbital = orbitalAt(craft->location);
        if (orbital)
        {
            onCaptureOrbital(orbital, craft->faction_id);
        }
    }
}

void Game::onWorkComplete(Craft *craft)
{
    // Its one caller guards this, but keep the precondition local: a -1 index is autopilot
    // cargo loading, which has no pod effect, and indexing on it reads before the array.
    if (!craft || craft->active_pod_index < 0)
    {
        return;
    }

    Pod &pod{craft->pods[craft->active_pod_index]};
    const Item &item{items[pod.contentType]};

    switch (item.id)
    {
    case ItemType::Of_Frame:
    {
        // if no orbital at location, create one.
        Orbital *orbital = orbitalAt(craft->location);
        if (!orbital)
        {
            orbital = createOrbital(craft->location);
        }
        if (++orbital->construction_progress >= REQUIRED_ORBITAL_FRAMES)
        {
            TraceLog(LOG_INFO, "Orbital construction complete at location %s", craft->location->name);
            orbital->operational = true;
        }
        raiseOrbitalConstructionEvent(orbital);
        // remove pod content
        pod.amount = 0;
    }
    break;
    case ItemType::R_Frame:
    {
        ResourceFacility *rf = resourceFacilityAt(craft->location);
        if (!rf)
        {
            rf = createResourceFacility(craft->location);
        }
        if (++rf->construction_progress >= REQUIRED_RESOURCE_FACILITY_FRAMES)
        {
            TraceLog(LOG_INFO, "Resource facility construction complete at location %s", craft->location->name);
            rf->operational = true;
        }
        raiseResourceFacilityConstructionEvent(rf);
        // remove pod content
        pod.amount = 0;
    }
    break;
    // grapple - must be in orbit, and have something to take/release
    // AMA - must be in orbit, at location of type asteroids
    // bandaid - bust be docked on surface, and have damaged facility
    case ItemType::Bandaid:
    {
        // handled in updateActivePod, which sets work time to 0 when complete, so this is just a notification
    }
    break;
    default:
        TraceLog(LOG_ERROR, "Completed work for unknown item ID %d ", item.id);
    }
}

void Game::onWorkCancelled(Craft *craft)
{
}

void Game::onCaptureOrbital(Orbital *orbital, int new_faction_id)
{
    if (!orbital)
    {
        TraceLog(LOG_ERROR, "Null orbital provided to onCaptureOrbital");
        return;
    }
    orbital->faction_id = new_faction_id;
    TraceLog(LOG_INFO, "Orbital at location %s captured by faction %d", orbital->body()->name, new_faction_id);

    // TODO
    // methanoids tend to trash orbitals before they are captured
    // transferring out most resources and surface derricks
}

void Game::addEventSink(EventSink *sink)
{
    if (sink)
    {
        eventSinks.push_back(sink);
    }
}

void Game::removeEventSink(EventSink *sink)
{
    eventSinks.erase(std::remove(eventSinks.begin(), eventSinks.end(), sink), eventSinks.end());
}

void Game::raiseLogEvent(const char *log_text)
{
    for (auto sink : eventSinks)
    {
        sink->addLog(log_text);
    }
}

void Game::raiseOrbitalConstructionEvent(Orbital *orbital)
{
    for (auto sink : eventSinks)
    {
        sink->onOrbitalConstruction(orbital);
    }
}

void Game::raiseResourceFacilityConstructionEvent(ResourceFacility *rf)
{
    for (auto sink : eventSinks)
    {
        sink->onResourceFacilityConstruction(rf);
    }
}

void Game::raiseProductionCompleteEvent(Factory *factory, int item_id)
{
    TraceLog(LOG_INFO, "Production complete: item %d", item_id);
    for (auto sink : eventSinks)
    {
        sink->onProductionComplete(factory, item_id);
    }
}

bool Game::processConsoleCommand(const char *command, Location *l, Facility *f)
{
    TraceLog(LOG_INFO, "Console command entered: %s", command);

    int item_id{ItemType::None};
    int amount{0};

    if (std::strcmp(command, "help") == 0)
    {
        TraceLog(LOG_INFO, "Available commands:\n"
                           "help - show this message\n"
                           "give {item_id} {amount} - add items to current facility\n"
                           "orbital - create an orbital at the current location for testing\n"
                           "shuttle - spawn shuttle at current facility for testing\n"
                           "research {topic_id} - set research progress for a topic (use -1 for all topics)");
        return true;
    }
    else if (std::sscanf(command, "give %d %d", &item_id, &amount) == 2) // give {item id} {amount} command to add items to current location for testing
    {
        if (f)
        {
            f->stores.items[item_id] += amount;
            TraceLog(LOG_INFO, "Added %d of item %d to current location %s", amount, item_id, f->body()->name);
            return true;
        }
    }
    // 'orbital' command to create an orbital at the current location for testing, if not present
    else if (std::strcmp(command, "orbital") == 0)
    {
        if (!orbitalAt(l))
        {
            Orbital *orbital{createOrbital(l)};
            orbital->operational = true;
            TraceLog(LOG_INFO, "Orbital created at location: %s", l->name);
            return true;
        }
    }
    // 'shuttle' to spawn shuttle at current facility if any
    else if (std::strcmp(command, "shuttle") == 0)
    {
        if (f && !locationHasShuttle(f->body())) // shuttles hang off the body, not the region
        {
            Shuttle *shuttle{createShuttle(f->body())};
            if (shuttle)
            {
                setDefaultRoute(shuttle, f); // debug spawn, so no chassis cost
            }
            TraceLog(LOG_INFO, "Shuttle created at facility: %s", f->body()->name);
            return true;
        }
    }
    // research {topic id} command to set research progress for testing
    else if (std::sscanf(command, "research %d", &item_id) == 1)
    {
        if (item_id == -1)
        {
            // set all research to complete
            for (auto &topic : researchTopics)
            {
                topic.progress = topic.requiredTime;
            }
            // set all items to researched as well
            for (auto &item : items)
            {
                item.researched = true;
            }
            TraceLog(LOG_INFO, "All research topics set to complete");
        }
        else if (item_id >= 0 && item_id < researchTopics.size())
        {
            researchTopics[item_id].progress = researchTopics[item_id].requiredTime;
            TraceLog(LOG_INFO, "Research topic %d set to complete", item_id);
        }
        return true;
    }

    return false;
}

void Game::setFactionHostility(int faction_id, bool hostile)
{
    if (faction_id >= 0 && faction_id < factions.size())
    {
        factions[faction_id].hostile = hostile;
        TraceLog(LOG_INFO, "Faction %d hostility set to %s", faction_id, hostile ? "true" : "false");
    }
    else
    {
        TraceLog(LOG_ERROR, "Invalid faction_id %d in setFactionHostility", faction_id);
    }
}

bool Game::hostilesAt(Location *location, int faction_id)
{
    if (!location)
    {
        TraceLog(LOG_ERROR, "Null location provided to hostilesAt");
        return false;
    }

    auto orbital = orbitalAt(location);
    if (orbital && orbital->faction_id != faction_id)
    {
        return factions[orbital->faction_id].hostile;
    }

    return false;
}