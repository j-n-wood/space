#pragma once

#include <memory>

#include "state/craft.h"

class Shuttle : public Craft
{
public:
    using Craft::Craft; // inherit constructor

    void update(); // update one tick
};

typedef std::unique_ptr<Shuttle> ShuttlePtr;