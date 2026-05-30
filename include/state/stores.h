#pragma once

#include <cstdint>
#include "state/resources.h"
#include "state/item.h"

// stores - resources and items
// source for production to draw from
// target for production to send to
// target for RF to send to

class Stores
{
public:
    int resources[ResourceType::Count];
    int items[ItemType::MAX_ITEM_TYPE];

    Stores();
};