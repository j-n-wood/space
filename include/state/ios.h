#pragma once

#include <memory>
#include <vector>

#include "state/craft.h"

class IOS : public Craft
{
public:
    IOS(CraftState cs, uint8_t mp, Location *loc, int fid) : Craft(cs, mp, loc)
    {
        type = CT_IOS;
        faction_id = faction_id;
    }
};

typedef std::unique_ptr<IOS> IOSPtr;
typedef std::vector<IOSPtr> IOSs;