#pragma once

#include "state/string_caps.h"

class Faction
{
public:
    int id; // database ID for loading/saving
    char name[NAME_MAX_LEN];

    Faction() : id{0}
    {
        name[0] = '\0';
    }

    Faction(int i, const char *n);
};