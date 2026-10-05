#include "state/game.h"
#include "state/faction.h"
#include "state/craft.h"
#include "state/crew.h"
#include "state/realtime_event.h"

bool Game::craftIsUnderAttack(Craft *craft)
{
    // iterate realtime events for an attack event
    for (const auto &event : realtime_events)
    {
        if (event->type == RealtimeEventType::Craft_Under_Attack && static_cast<CraftRealtimeEvent *>(event.get())->craft == craft)
        {
            return true;
        }
    }
    return false;
}

Craft *Game::targetCraftAt(int faction_id, Location *location)
{
    // if the faction is not hostile, there is no target
    if (!factionIsHostile(faction_id))
    {
        return nullptr;
    }

    // iterate through all craft in the location and return the first one that is a valid target
    for (auto &craft : ios)
    {
        if (craft->location == location && craft->faction_id != faction_id && !craft->moving())
        {
            return craft.get();
        }
    }
    // TODO SCG
    return nullptr;
}

Craft *Game::spawnWarship(int faction_id, Location *location, int crew_rank, int initial_drones)
{
    // implementation for spawning a warship at the given location
    Craft *warship = createIOS(location);
    // set cargo to DFCC
    warship->pods[0].type = PodType::PT_WEAPON;
    warship->pods[0].contentType = ItemType::DFCC;
    warship->pods[0].amount = initial_drones;
    warship->faction_id = faction_id;

    // create a crew - faction dependent, which we have not done
    // warship->crew = createCrew(0, CrewType::Marine, "Leader Name", crew_rank, 40, 0.0f);
    return warship;
}