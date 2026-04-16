#include "state/stores.h"

Stores::Stores()
{
    for (int idx = 0; idx < ResourceType::Count; ++idx)
    {
        resources[idx] = 0;
    }

    for (int idx = 0; idx < MAX_ITEM_TYPE; ++idx)
    {
        items[idx] = 0;
    }
}