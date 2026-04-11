#include "state/game.h"

Game::Game() : currentSystem(nullptr), currentLocation(nullptr), currentFacility(nullptr), game_time(0)
{
    for (int i = 0; i < GAME_MAX_SYSTEMS; i++)
    {
        systems[i] = nullptr;
    }
}

Game::~Game()
{
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

void Game::update()
{
    // update all facilities
    for (auto &base : bases)
    {
        base->update();
    }

    ++game_time;
}