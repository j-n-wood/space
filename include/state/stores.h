#pragma once

#include <cstdint>
#include "state/resources.h"

// stores - resources and items
// source for production to draw from
// target for production to send to
// target for RF to send to

const int MAX_ITEM_TYPE = 32; // for now. Dynamic from data file

class Stores
{
public:
    int resources[ResourceType::Count];
    int items[MAX_ITEM_TYPE];

    Stores();
};