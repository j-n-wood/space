#include <algorithm>
#include "state/craft.h"
#include "state/game.h"
#include "state/autopilot.h"

Autopilot::Autopilot() : state{AS_OFF}, flow{}
{
    for (int i = 0; i < MAX_DESTINATIONS; ++i)
    {
        cursors[i] = 0;
    }
}

Autopilot::~Autopilot()
{
}

int Autopilot::nextFlagged(uint8_t mask, uint8_t *cursor) const
{
    for (uint8_t step = 0; step < ResourceType::Count; ++step)
    {
        uint8_t idx = (*cursor + step) % ResourceType::Count;
        if (flow[idx] & mask)
        {
            *cursor = (idx + 1) % ResourceType::Count;
            return idx;
        }
    }
    return -1;
}

void Autopilot::onDocked(Craft *craft)
{
    if (state < AS_ON)
    {
        return;
    }

    // have arrived, generally at destination
    auto game = Game::getCurrent();

    if (true) // TODO: validate we really are at destination
    {
        Facility *current = game->facilityAt(craft->destinations[craft->destination_index]);
        Facility *other = game->facilityAt(craft->destinations[(craft->destination_index + 1) % MAX_DESTINATIONS]);

        // which cursor counts as current, if current changes on arrival?
        // A: source is surface for shuttles
        // for IOS/SCG, should we switch cursors on launch?
        // maybe tag cursors to facility and reset to 0 on facility change?
        uint8_t *current_cursor = &cursors[craft->destination_index];
        uint8_t mask = craft->destination_index == 0 ? RF_LOAD_AT_DEST : RF_LOAD_AT_SOURCE;

        if (!current || !other)
        {
            // TODO this could happen if source/dest orbital destroyed.
            // Turn off in this case
            return;
        }

        // How much of resource `idx` would we want to put in a pod right now,
        // given which end of the run we are at. Balance splits the difference;
        // one-way takes a pod-full.
        auto desiredAmount = [&](int idx)
        {
            if ((flow[idx] & RF_BALANCE) == RF_BALANCE)
            {
                int diff = current->stores.resources[idx] - other->stores.resources[idx];
                return std::min(std::max(diff / 2, 0), MAX_SUPPLY_POD_AMOUNT);
            }
            return MAX_SUPPLY_POD_AMOUNT;
        };

        for (int pod_idx = 0; pod_idx < craft->max_pods; ++pod_idx)
        {
            Pod &pod = craft->pods[pod_idx];
            if (pod.type != PT_SUPPLY)
            {
                continue;
            }
            game->setSupplyPodContent(&pod, &current->stores, -1, 0);

            int r = nextFlagged(mask, current_cursor, [&](int idx)
                                { return desiredAmount(idx) > 0 && current->stores.resources[idx] > 0; });
            if (r < 0)
            {
                continue;
            }
            game->setSupplyPodContent(&pod, &current->stores, r, desiredAmount(r));
        }

        // mark craft as working
        craft->work(1.0f);
    } // at dest
}

void Autopilot::onDockWorkComplete(Craft *craft)
{
    craft->launch();
}

void Autopilot::update(Craft *craft, float delta)
{
    if (state < AS_ON)
    {
        return;
    }

    // dock if arrived at orbit, and endpoint requires docking.

    // Note: IOS should not be set to have destination_state of CS_SURFACE_DOCKED
    if (craft->state == CS_ORBIT)
    {
        auto &dest{craft->currentDestination()};
        // are we at desired location?
        if (dest.location == craft->location)
        {
            if ((dest.sublocation == SLOC_ORBIT) && (dest.docked))
            {
                craft->state = CS_ORBIT_DOCKING;
                craft->state_timer = CSTD_DOCK;
            }
            else if ((dest.sublocation == SLOC_SURFACE) && (dest.docked))
            {
                craft->state = CS_DESCENDING;
                craft->state_timer = CSTD_DESCENT;
            }
        }
        else
        {
            // location is different. start transit
            craft->engageDrive();
        }
    }
}
