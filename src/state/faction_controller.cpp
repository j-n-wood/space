#include "state/faction_controller.h"
#include "state/game.h"

#include <vector>

std::vector<Orbital *> FactionController::systemOrbitals(const int system_id)
{
    Game *game = Game::getCurrent();
    std::vector<Orbital *> result;
    for (auto &o : game->allOrbitals())
    {
        if ((o->faction_id == this->faction_id) && (o->system->id == system_id) && (o->operational) && (!o->destroyed))
        {
            result.push_back(o);
        }
    }
    return result;
}

std::vector<Craft *> FactionController::systemWarships(const int system_id)
{
    Game *game = Game::getCurrent();
    std::vector<Craft *> result;
    for (auto &i : game->allIOS())
    {
        if ((i->faction_id == this->faction_id) && (i->location->system->id == system_id) && (i->isWarship()))
        {
            result.push_back(i.get());
        }
    }
    return result;
}

Location *FactionController::selectTargetLocation(Craft *warship)
{
    // Implement the logic to select a target location for the given warship
    // Placeholder implementation: return nullptr for now
    Location *target = nullptr;
    float transit_time = 10000.0f; // long
    Game *game = Game::getCurrent();
    int system_id = warship->location->system->id;
    for (auto &t : game->allOrbitals())
    {
        if ((t->faction_id == faction_id) || (t->system->id != system_id) || t->destroyed || (!t->operational))
        {
            continue; // skip orbitals belonging to the same faction, or different system
            // TODO - interstellar travel considerations
        }
        float t_transit_time = game->transitTimeCalculator->calculateTransitTime(warship->location, t);
        if (t_transit_time < transit_time)
        {
            transit_time = t_transit_time;
            target = t;
        }
    }
    return target;
}

int FactionController::warshipRank() const
{
    // Determine the warship rank based on the number of captured orbitals
    Game *game = Game::getCurrent();
    const Faction &faction = game->factions[FACTION_TERRAN];
    if (faction.captured_orbitals >= 12)
    {
        return 4;
    }
    else if (faction.captured_orbitals >= 8)
    {
        return 3;
    }
    else if (faction.captured_orbitals >= 5)
    {
        return 2;
    }
    else
    {
        return 1;
    }
}

int FactionController::garrisonThreshold() const
{
    // how many drones an orbital should maintain as its garrison
    // excess are sent to build fleets
    // driven by enemy success
    Game *game = Game::getCurrent();
    const Faction &faction = game->factions[FACTION_TERRAN];
    if (faction.captured_orbitals >= 12)
    {
        return 200;
    }
    else if (faction.captured_orbitals >= 8)
    {
        return 150;
    }
    else if (faction.captured_orbitals >= 5)
    {
        return 100;
    }
    else
    {
        return 70;
    }
}

void FactionController::update()
{
    Game *game{Game::getCurrent()};
    if (!game->factions[FACTION_METHANOID].hostile)
    {
        return;
    }

    // produce drones based on orbitals that are active
    // methanoid orbitals all (start with) AOC so can produce
    // if we queue an item and mark it as 'started', resource checks are not done
    // we can use this to drive standard factories rather than magic production

    // iterate orbitals, produce drones, send overproduction to one orbital per system
    // when sufficient are available, spawn a warship and send to engage player orbitals in the same
    // system. If no such orbital exists, send interstellar.

    // when producing drones, rather than wait for full garrison, we can transfer _some_ drones as
    // excess to build a fleet.
    // Also, if there is a fleet at an orbital, it has some defense as long as that fleet is present.

    // For reference, original logic appeared to:
    // Run a reinforcement cycle every 7-10 ticks
    // Each produces 2 drones per orbital
    // Also each system gets one drone for fleet reinforcement, or 2 if at least one orbital has full strength
    // Fleets are then sent on attacks - each cycle increments a system counter to randomise a bit
    // This means production is ~ 2x player (8 ticks) per orbital, but there is no collection from multiple orbitals to fleets
    // 700-1000 ticks to get a complete fleet (doesn't seem right, but hey)

    std::vector<int> system_drone_counts(game->allSystems().size(), 0);
    std::vector<Location *> system_spawn_locations(game->allSystems().size(), nullptr);

    // locate faction orbitals

    int garrison_threshold = garrisonThreshold();

    for (auto &o : game->allOrbitals())
    {
        if (o->faction_id != faction_id)
        {
            continue;
        }
        if (o->destroyed || !o->operational)
        {
            continue;
        }
        // record a spawn location for this system if not already set
        if (!system_spawn_locations[o->system->id])
        {
            system_spawn_locations[o->system->id] = o->body()->orbit();
        }
        if (!o->factory->isActive())
        {
            // queue an item to start production
            o->factory->queueItem(ItemType::Ios_Drone);
            o->factory->queue.back().started = true; // hack around actual resource requirements for now
        }

        // if we have sufficient drones, produce a warship
        // this can depend on player strength and progress to set fleet strength, rank
        // accumulate per system - using excess drones over max of 200
        // storage location is not considered as emeny factions have MTX by default
        if (o->stores.items[ItemType::Ios_Drone] > garrison_threshold)
        {
            system_drone_counts[o->system->id] += o->stores.items[ItemType::Ios_Drone] - garrison_threshold;
            o->stores.items[ItemType::Ios_Drone] = garrison_threshold; // reduce to max after counting excess
        }
    }

    // per system, is it time to produce a warship based on drone counts?
    // if nonzero, build/reinforce a warship for that system. Accumulate excess drones towards the next warship

    std::vector<std::vector<Craft *>> warships(game->allSystems().size(), std::vector<Craft *>());

    for (auto &i : game->allIOS())
    {
        if (i->faction_id != faction_id)
        {
            continue;
        }
        if (!i->isWarship())
        {
            continue;
        }

        warships[i->location->system->id].push_back(i.get());
    }

    for (size_t system_id = 0; system_id < system_drone_counts.size(); ++system_id)
    {
        auto &system_warships = warships[system_id];
        if (system_drone_counts[system_id] > 0)
        {
            TraceLog(LOG_INFO, "System %zu has %d excess drones", system_id, system_drone_counts[system_id]);

            // add to all warships in this system
            // if some remain, spawn a new warship

            for (auto &i : system_warships)
            {
                if ((i->pods[0].amount < MAX_DRONE_FLEET_SIZE) && i->idle())
                {
                    int reinforce_amount = std::min(system_drone_counts[system_id], MAX_DRONE_FLEET_SIZE - i->pods[0].amount);
                    i->pods[0].amount += reinforce_amount;
                    system_drone_counts[system_id] -= reinforce_amount;
                }
            }

            // any left?
            if (system_drone_counts[system_id] > 0)
            {
                TraceLog(LOG_INFO, "System %zu has %d leftover drones after reinforcing existing warships", system_id, system_drone_counts[system_id]);
                if (system_spawn_locations[system_id])
                {
                    while (system_drone_counts[system_id] > 0)
                    {
                        // if there is a target ship at this location, it could be engaged immediately
                        TraceLog(LOG_INFO, "Spawning new warship in system %zu at location %s due to leftover drones", system_id, system_spawn_locations[system_id]->name);
                        auto warship = game->spawnWarship(faction_id, system_spawn_locations[system_id], warshipRank(), 0);
                        warship->pods[0].amount += std::min(MAX_DRONE_FLEET_SIZE, system_drone_counts[system_id]);
                        system_drone_counts[system_id] -= std::min(MAX_DRONE_FLEET_SIZE, system_drone_counts[system_id]);
                        system_warships.push_back(warship);
                    } // reinforce with new warships while there are drones
                }
            }
        }

        for (auto &i : system_warships)
        {
            if ((i->pods[0].amount == MAX_DRONE_FLEET_SIZE) && (!i->moving()) && (i->canEngageDrive()) && (!i->engagement))
            {
                // select a target and engage drive
                // use game transit time calculation - initially just pick the shortest time
                Location *target = selectTargetLocation(i);
                if (!target)
                {
                    TraceLog(LOG_INFO, "Warship in system %zu has no valid target", system_id);
                    continue;
                }

                TraceLog(LOG_INFO, "Warship in system %zu engaging target %s", system_id, target->name);
                i->setDestination(i->destination_index, target);
                i->engageDrive();
                // could give a 'head start' but will try actual simulation first.
                // interstellar could behave differently (or just start faction with better drives)
            }
        }
    }
}