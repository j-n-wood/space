#include "state/game.h"
#include "loaders/load_system.h"
#include "state/training_facility.h"

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

    if (!loader->loadFactions())
    {
        TraceLog(LOG_ERROR, "Failed to load factions");
        return false;
    }

    if (!loader->loadCrews())
    {
        TraceLog(LOG_ERROR, "Failed to load crews");
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

    // load before craft that can reference them
    if (!loader->loadObjects())
    {
        TraceLog(LOG_ERROR, "Failed to load objects");
        return false;
    }

    if (!loader->loadFactoryQueues())
    {
        TraceLog(LOG_ERROR, "Failed to load factory queues");
        return false;
    }

    if (!loader->loadResearchTopics())
    {
        TraceLog(LOG_ERROR, "Failed to load research topics");
        return false;
    }

    if (!loader->loadResearchFacilities())
    {
        TraceLog(LOG_ERROR, "Failed to load research facilities");
        return false;
    }

    if (!loader->loadCraft())
    {
        TraceLog(LOG_ERROR, "Failed to load craft");
        return false;
    }

    // cross-reference hacks // TODO

    // crew in training -> EC facility
    for (auto &crew : crews)
    {
        if (crew->inTraining())
        {
            if (!earth_city || !earth_city->training_facility)
            {
                TraceLog(LOG_ERROR, "No Earth City training facility for crew %d in training", crew->id);
                return false;
            }
            switch (crew->type)
            {
            case CrewType::Scientist:
                earth_city->training_facility->scientists = crew.get();
                break;
            case CrewType::Engineer:
                earth_city->training_facility->engineers = crew.get();
                break;
            case CrewType::Marine:
                earth_city->training_facility->marines = crew.get();
                break;
            default:
                TraceLog(LOG_ERROR, "Unknown crew type %d for crew %d in training", static_cast<int>(crew->type), crew->id);
                return false;
            }
        }
    }

    TraceLog(LOG_INFO, "Game initialisation complete");

    return true;
}