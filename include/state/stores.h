#pragma once

#include <cstdint>
#include "state/resources.h"

// stores - resources and items
// source for production to draw from
// target for production to send to
// target for RF to send to

class Stores
{
public:
    uint32_t resources[ResourceType::Count];

    Stores();
};