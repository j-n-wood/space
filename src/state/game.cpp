#include "loaders/loader.h"
#include "state/game.h"

std::unique_ptr<Game> Game::current;

const float MAX_TIMESTEP = 1.0f;

Game::Game() : game_time(0.0f), time_rate(1.0f)
{
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

EarthCity *Game::createEarthCity(Location *location)
{
    bases.emplace_back(std::make_unique<EarthCity>(location));
    return static_cast<EarthCity *>(bases.back().get());
}

ResourceFacility *Game::createResourceFacility(Location *location)
{
    bases.emplace_back(std::make_unique<ResourceFacility>(location));
    return bases.back().get();
}

ResourceFacility *Game::resourceFacilityAt(Location *location)
{
    if (location)
    {
        // look for resource facility at given location. Initial impl is scan (yuck)
        // avoid by e.g. lookup based on location to init UI controls, keep as UI state.
        for (auto &base : bases)
        {
            if (base->location == location)
            {
                return base.get();
            }
        }
    }
    return nullptr;
}

Orbital *Game::createOrbital(Location *location)
{
    orbitals.emplace_back(std::make_unique<Orbital>(location));
    auto o = orbitals.back().get();
    return o;
}

Orbital *Game::orbitalAt(Location *location)
{
    if (location)
    {
        // look for resource facility at given location. Initial impl is scan (yuck)
        // avoid by e.g. lookup based on location to init UI controls, keep as UI state.
        for (auto &orbital : orbitals)
        {
            if (orbital->location == location)
            {
                return orbital.get();
            }
        }
    }
    return nullptr;
}

Facility *Game::facilityAt(const Endpoint &endpoint)
{
    if (endpoint.location)
    {
        if (endpoint.sublocation == SLOC_SURFACE)
        {
            return resourceFacilityAt(endpoint.location);
        }
        return orbitalAt(endpoint.location);
    }
    return nullptr;
}

Factory *Game::createFactory(Facility *facility)
{
    Factory *f{facility->createFactory()};
    factories.push_back(f);
    return f;
}

bool Game::canCommissionShuttle(Facility *facility) const
{
    // require ios_chassis item, and no existing shuttle at location
    if (!facility || !facility->location)
    {
        return false;
    }
    if (facility->location->shuttle)
    {
        return false;
    }
    return facility->stores.items[ItemType::S_Chassis] > 0;
}

bool Game::canCommissionIOS(Facility *facility) const
{
    if (!facility || !facility->location)
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
    auto shuttle = createShuttle(facility);
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

Shuttle *Game::createShuttle(Facility *facility)
{
    // create a shuttle at a location, starting at the given facility
    Location *location{facility->location};
    if (!location)
    {
        TraceLog(LOG_ERROR, "Facility missing location");
        return nullptr;
    }
    if (location->shuttle)
    {
        TraceLog(LOG_ERROR, "Blocked createShuttle: location already has one", location->name);
        return nullptr;
    }
    // initial state depends on facility sublocation
    CraftState cs{facility->sublocation == SLOC_ORBIT ? CS_ORBIT_DOCKED : CS_SURFACE_DOCKED};

    location->shuttle = std::move(std::make_unique<Shuttle>(cs, 1, location));
    shuttles.push_back(location->shuttle.get());
    // initial destination is the other kind of sublocation
    location->shuttle->destinations[0] = Endpoint(location, SublocationType(1 - facility->sublocation), true);
    location->shuttle->destinations[1] = Endpoint(location, facility->sublocation, true);
    return location->shuttle.get();
}

IOS *Game::createIOS(Facility *facility)
{
    // create an IOS at a location, starting at the given facility
    Location *location{facility->location};
    if (!location)
    {
        TraceLog(LOG_ERROR, "Facility missing location");
        return nullptr;
    }

    ios.emplace_back(std::make_unique<IOS>(CS_ORBIT_DOCKED, 3, location));
    auto i = ios.back().get();
    i->destinations[0] = Endpoint(location, SLOC_ORBIT, true);
    i->destinations[1] = Endpoint(location, SLOC_ORBIT, true);

    // generate a name based on creation count
    std::snprintf(i->name, sizeof i->name, "IOS-%04d", ios_number++);

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
    for (auto shuttle : shuttles)
    {
        shuttle->update(dt);
    }

    for (auto &craft : ios)
    {
        craft->update(dt);
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