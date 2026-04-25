#pragma once

#include <memory>
#include <vector>

#include "state/craft.h"

class IOS : public Craft
{
public:
    IOS(CraftState cs, uint8_t mp, Location *loc) : Craft(cs, mp, loc)
    {
        type = CT_IOS;
    }

    void update(float delta) override; // update by game time
};

typedef std::unique_ptr<IOS> IOSPtr;
typedef std::vector<IOSPtr> IOSs;