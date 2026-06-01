#include "state/orbital.h"
#include "state/game.h"

Orbital::Orbital(Location *l) : Facility{l}
{
    sublocation = SLOC_ORBIT; // default, but make sure
}

void Orbital::update()
{
    // factory updated independently

    // if have MTX, can beam resources from ground facility
    if (mtx_installed && location)
    {
        auto rf = Game::getCurrent()->resourceFacilityAt(location);
        if (rf)
        {
            // transfer some resources up to orbital, up to available amount and orbital storage capacity
            for (int i = 0; i < ResourceType::Count; ++i)
            {
                int available = rf->stores.resources[i];
                int capacity = MAX_ORBITAL_STORAGE - stores.resources[i]; // use the defined constant for orbital storage capacity
                int transfer_amount = std::min(available, capacity);
                if (transfer_amount > 0)
                {
                    rf->stores.resources[i] -= transfer_amount;
                    stores.resources[i] += transfer_amount;
                }
            }
        }
    }
}