#pragma once

#include "state/string_caps.h"
#include "state/resources.h"

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

    FactionTrade tradeTable[ResourceType::Count];

    Faction() : id{0}, hostile{false}, trades{0}
    {
        name[0] = '\0';
    }

    Faction(int i, const char *n);
};

enum FactionInteraction
{
    Unset,
    BeforeCommspod,     // message saying bring a grapple
    GiveCommspod,       // message saying gave a commspod
    Trade,              // trade negotiations
    SympatheticWarning, // warning of war
    DeclareWar,         // declaration of war
    Max
};