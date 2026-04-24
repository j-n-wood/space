#pragma once

#include <memory>

#include "state/craft.h"

class Shuttle : public Craft
{
public:
    using Craft::Craft; // inherit constructor

    void update(float delta) override; // update by game time
};

typedef std::unique_ptr<Shuttle> ShuttlePtr;