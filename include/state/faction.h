#pragma once

#include "state/string_caps.h"
#include "state/resources.h"

// default values

const int FACTION_TERRAN = 0;
const int FACTION_METHANOID = 1;

class FactionTrade
{
public:
    int traded_resource_id;
    float rate;

    FactionTrade() : traded_resource_id{0}, rate{0.0f} {}
};

class Faction
{
public:
    int id; // database ID for loading/saving
    char name[NAME_MAX_LEN];
    bool hostile;
    int trades;
    int ios_number;
    int scg_number;

    FactionTrade tradeTable[ResourceType::Count];

    Faction() : id{0}, hostile{false}, trades{0}, ios_number{1}, scg_number{1}
    {
        name[0] = '\0';
    }

    Faction(int i, const char *n);
};
