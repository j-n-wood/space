#include <algorithm>
#include "state/craft.h"
#include "state/game.h"
#include "state/autopilot.h"
#include "state/craft_action.h"

const char *autopilotStateNames[AS_COUNT] = {
    "Disabled",
    "Off",
    "On",
    "Complete",
};

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
        // The endpoints name the facilities directly, so no reconstruction is needed.
        Facility *current = asFacility(craft->destinations[craft->destination_index].location);
        Facility *other = asFacility(craft->destinations[(craft->destination_index + 1) % MAX_DESTINATIONS].location);

        // which cursor counts as current, if current changes on arrival?
        // A: source is surface for shuttles
        // for IOS/SCG, should we switch cursors on launch?
        // maybe tag cursors to facility and reset to 0 on facility change?
        uint8_t *current_cursor = &cursors[craft->destination_index];
        uint8_t mask = craft->destination_index == 0 ? RF_LOAD_AT_SOURCE : RF_LOAD_AT_DEST;

        if (!current || !other)
        {
            // TODO this could happen if source/dest orbital destroyed.
            // Turn off in this case
            TraceLog(LOG_WARNING, "Autopilot: %s docked but missing source or destination facility, disabling autopilot", craft->name);
            state = AS_OFF;
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

    // Only ever act on a craft at rest. Without this the autopilot re-issues its
    // command every tick of a manoeuvre -- descend() resetting state_timer each frame
    // means the descent never completes and the craft hangs in orbit forever. The
    // 14-state model got this free from `state != CS_ORBIT`; with position out of the
    // enum it has to be asked directly.
    if (craft->moving() || craft->isWorking())
    {
        return;
    }

    // Docked is not ours to act on: onDocked starts the work, and onDockWorkComplete
    // launches when it finishes. Anything else undocked is a leg to fly.
    if (craft->docked())
    {
        return;
    }

    auto &dest{craft->currentDestination()};
    if (!dest.location || !craft->location)
    {
        return;
    }

    // Same BODY means a local manoeuvre -- ascend, descend or dock. A different body is
    // what makes a leg a transit. Comparing exact locations here would fire the
    // interplanetary drive for a surface-to-orbit hop, since the craft sits in a region
    // while the endpoint names a facility.
    if (dest.location->body() != craft->location->body())
    {
        craft->engageDrive();
        return;
    }

    // Local. Both sides matter: where the destination is, and which side the craft is
    // on now. Keying off the destination alone left a craft that had just launched from
    // a surface station sitting in the surface region with nothing to move it.
    if (dest.location->inOrbit())
    {
        if (craft->location->isOnSurface() && craft->hasCapability(CC_ATMOSPHERIC))
        {
            TraceLog(LOG_INFO, "Autopilot: %s ascending to orbit", craft->name);
            craft->ascend();
        }
        else if (dest.location->isFacility())
        {
            TraceLog(LOG_INFO, "Autopilot: %s docking at destination orbit", craft->name);
            craft->dock();
        }
        else
        {
            TraceLog(LOG_WARNING, "Autopilot: %s at destination orbit but no station to dock with", craft->name);
        }
    }
    else if (craft->inOrbit() && (dest.location->isOnSurface()) && (craft->hasCapability(CC_ATMOSPHERIC)))
    {
        TraceLog(LOG_INFO, "Autopilot: %s descending to surface", craft->name);
        craft->descend();
    }
    // else: already on the surface side. Descent docks on arrival if there is a station,
    // so there is nothing further to command from here.
}
