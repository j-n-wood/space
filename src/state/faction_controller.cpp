#include "state/faction_controller.h"
#include "state/game.h"

#include <vector>

std::vector<const Orbital *> FactionController::systemOrbitals(const int system_id)
{
    Game *game = Game::getCurrent();
    std::vector<const Orbital *> result;
    for (auto &o : game->allOrbitals())
    {
        if ((o->faction_id == this->faction_id) && (o->system->id == system_id) && (o->operational) && (!o->destroyed))
        {
            result.push_back(o);
        }
    }
    return result;
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

    std::vector<int> system_drone_counts(game->allSystems().size(), 0);
    std::vector<Location *> system_spawn_locations(game->allSystems().size(), nullptr);

    // locate faction orbitals
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
        if (o->stores.items[ItemType::Ios_Drone] > 200)
        {
            system_drone_counts[o->system->id] += o->stores.items[ItemType::Ios_Drone] - 200;
            o->stores.items[ItemType::Ios_Drone] = 200; // reduce to max after counting excess
        }
    }

    // per system, is it time to produce a warship based on drone counts?
    // if nonzero, build/reinforce a warship for that system. Accumulate excess drones towards the next warship

    std::vector<Craft *> warships(game->allSystems().size(), nullptr);

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
        // assign the warship to its system slot if not already assigned
        if (!warships[i->location->system->id])
        {
            warships[i->location->system->id] = i.get();
        }
    }

    for (size_t system_id = 0; system_id < system_drone_counts.size(); ++system_id)
    {
        while (system_drone_counts[system_id] > 0)
        {
            TraceLog(LOG_INFO, "System %zu has %d excess drones", system_id, system_drone_counts[system_id]);

            // while we have drones, and less than the 200 cap, spawn warships and reinforce existing ones
            // check if there is one already
            if (!warships[system_id] && system_spawn_locations[system_id])
            {
                // build a new warship for this system
                // reduce the drone count by the number used for the warship
                // pick a spawn location from that system
                TraceLog(LOG_INFO, "Spawning new warship in system %zu at location %s", system_id, system_spawn_locations[system_id]->name);
                warships[system_id] = game->spawnWarship(faction_id, system_spawn_locations[system_id], 2, 0);
            }

            if (warships[system_id])
            {
                // reinforce the existing warship with available drones, up to 200
                int reinforce_amount = std::min(system_drone_counts[system_id], 200 - game->droneCountForCraft(warships[system_id]));
                warships[system_id]->pods[0].amount += reinforce_amount;
                system_drone_counts[system_id] -= reinforce_amount;

                // enough to go attack something?
                if ((warships[system_id]->pods[0].amount == 200) && (!warships[system_id]->moving()))
                {
                    // select a target and engage drive
                    // use game transit time calculation - initially just pick the shortest time
                    Location *target = nullptr;
                    float transit_time = 10000.0f; // long
                    for (auto &t : game->allOrbitals())
                    {
                        if ((t->faction_id == faction_id) || (t->system->id != system_id))
                        {
                            continue; // skip orbitals belonging to the same faction, or different system
                            // TODO - interstellar travel considerations
                        }
                        float t_transit_time = game->transitTimeCalculator->calculateTransitTime(warships[system_id]->location, t);
                        if (t_transit_time < transit_time)
                        {
                            transit_time = t_transit_time;
                            target = t;
                        }
                    }

                    if (target)
                    {
                        TraceLog(LOG_INFO, "Warship in system %zu engaging target %s", system_id, target->name);
                        warships[system_id]->setDestination(0, target);
                        warships[system_id]->engageDrive();
                        // could give a 'head start' but will try actual simulation first.
                        // interstellar could behave differently (or just start faction with better drives)
                    }

                    // clear slot so we can spawn a new warship in the next iteration
                    warships[system_id] = nullptr;
                }
            }
        }
    }
}