#pragma once

#include <memory>

#include "state/craft.h"

class Shuttle : public Craft
{
public:
    Shuttle(CraftState cs, uint8_t mp, Location *loc);

    void update(float delta) override; // update by game time
};

typedef std::unique_ptr<Shuttle> ShuttlePtr;