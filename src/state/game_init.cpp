#include "state/game.h"
#include "loaders/load_system.h"

bool Game::initialise(Loader *loader)
{
    // Implementation for initialising the game to default state. Loads all common data, no save game data.

    TraceLog(LOG_INFO, "Initialising game");

    loader->setGame(this);

    if (!loader->loadSystems())
    {
        TraceLog(LOG_ERROR, "Failed to load systems");
        return false;
    }

    if (!loader->loadGame())
    {
        TraceLog(LOG_ERROR, "Failed to load game state");
        return false;
    }

    if (!loader->loadFacilities())
    {
        TraceLog(LOG_ERROR, "Failed to load facilities");
        return false;
    }

    if (!loader->loadStores())
    {
        TraceLog(LOG_ERROR, "Failed to load stores");
        return false;
    }

    if (!loader->loadItems())
    {
        TraceLog(LOG_ERROR, "Failed to load items");
        return false;
    }

    currentSystem = systems[0].get();

    return true;
}