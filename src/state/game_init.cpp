#include "state/game.h"
#include "loaders/load_system.h"

bool Game::initialise(Loader *loader)
{
    // Implementation for initialising the game to default state. Loads all common data, no save game data.

    TraceLog(LOG_INFO, "Initialising game");

    /*
    this->systems[0] = createSystem(false);
    if (!loadSystem(loader, 1, systems[0].get()))
    {
        TraceLog(LOG_ERROR, "Failed to load system");
        return false;
    }
    currentSystem = systems[0].get();
    */
    if (!loader->loadSystems())
    {
        TraceLog(LOG_ERROR, "Failed to load systems");
        return false;
    }

    currentSystem = systems[0].get();

    return true;
}