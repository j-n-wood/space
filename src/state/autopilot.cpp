#include <algorithm>
#include "state/craft.h"
#include "state/game.h"

Autopilot::Autopilot() : state{AS_OFF}, flow{}, surface_cursor{0}, orbit_cursor{0}, last_at_source{false}
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

void Autopilot::update(Craft *craft, float delta)
{
    if (state < AS_ON)
    {
        return;
    }

    auto game = Game::getCurrent();

    // this is set up for shuttles, craft added on - refactor
    bool isAtDest = false;
    bool isAtSource = false;
    if (craft->type != CT_SHUTTLE)
    {
        isAtDest = (craft->state == CS_ORBIT_DOCKED);
        isAtSource = (craft->state == CS_SURFACE_DOCKED);
    }
    else
    {
        isAtDest = (craft->state == CS_ORBIT_DOCKED) || (craft->location == craft->destination);
    }

    if (isAtSource || isAtDest)
    {
        Facility *orbital = game->orbitalAt(craft->location);
        Facility *facility = game->resourceFacilityAt(craft->location);
        Facility *current = isAtDest ? orbital : facility;
        Facility *other = isAtDest ? facility : orbital;
        uint8_t *current_cursor = isAtDest ? &orbit_cursor : &surface_cursor;
        uint8_t mask = isAtDest ? RF_LOAD_AT_DEST : RF_LOAD_AT_SOURCE;

        if (!orbital || !facility)
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

        // trigger craft launch
        if (craft->state == CS_SURFACE_DOCKED)
        {
            last_at_source = true;
            craft->state = CS_SURFACE_LAUNCH;
            craft->state_timer = CSTD_LAUNCH;
        }
        else if (craft->state == CS_ORBIT_DOCKED)
        {
            last_at_source = false;
            craft->state = CS_ORBIT_LAUNCH;
            craft->state_timer = CSTD_LAUNCH;
        }
    } // at source or dest

    if (craft->type == CT_SHUTTLE)
    {
        // other state transitions. Inititally handle shuttle case
        if (craft->state == CS_ORBIT)
        {
            if (last_at_source)
            {
                craft->state = CS_ORBIT_DOCKING;
                craft->state_timer = CSTD_DOCK;
            }
            else
            {
                craft->state = CS_DESCENDING;
                craft->state_timer = CSTD_DESCENT;
            }
        }
    }
    else
    {
        // if in orbit at dest, dock
        if (craft->state == CS_ORBIT)
        {
            if (craft->location == craft->destination)
            {
                craft->state = CS_ORBIT_DOCKING;
                craft->state_timer = CSTD_DOCK;
            }
        }
    }
}
