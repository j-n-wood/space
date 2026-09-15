#pragma once

#include <memory>
#include <vector>

#include "state/craft.h"

class Shuttle : public Craft
{
public:
    Shuttle(CraftState cs, uint8_t mp, Location *loc);

    void update(float delta) override; // update by game time
};

typedef std::unique_ptr<Shuttle> ShuttlePtr;
// Game owns shuttles, as it already owns IOS. Location::shuttle is a non-owning
// reference: a craft's position and its owner are separate things, and conflating
// them meant a docked shuttle could be reparented onto a facility by a save/load.
typedef std::vector<ShuttlePtr> Shuttles;