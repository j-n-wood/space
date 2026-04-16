#include "state/game.h"

std::unique_ptr<Game> Game::current;

const float MAX_TIMESTEP = 1.0f;

Game::Game() : game_time(0.0f)
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

Factory *Game::createFactory(Facility *facility)
{
    Factory *f{facility->createFactory()};
    factories.push_back(f);
    return f;
}

void Game::update(float delta)
{
    // add to time, if ticks over one second call advanceTick
    if (delta > MAX_TIMESTEP)
    {
        delta = MAX_TIMESTEP;
    }

    int prior{static_cast<int>(game_time)};
    game_time += delta;

    // start with updating all systems - could only update visible ones
    for (auto &system : systems)
    {
        system->update(game_time);
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