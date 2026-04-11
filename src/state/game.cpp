#include "state/game.h"

Game::Game() : currentSystem(nullptr), currentLocation(nullptr), currentFacility(nullptr), game_time(0)
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
    sys->name.assign(name);
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

ResourceFacility *Game::createResourceFacility(Location *location)
{
    bases.emplace_back(std::make_unique<ResourceFacility>(location));
    return bases.back().get();
}

ResourceFacility *Game::resourceFacilityAt(Location *location)
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
    return nullptr;
}

Orbital *Game::createOrbital(Location *location)
{
    orbitals.emplace_back(std::make_unique<Orbital>(location));
    auto o = orbitals.back().get();
    factories.push_back(&o->factory);
    return o;
}

Orbital *Game::orbitalAt(Location *location)
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
    return nullptr;
}

void Game::update()
{
    // update all facilities
    for (auto &base : bases)
    {
        base->update();
    }

    ++game_time;
}