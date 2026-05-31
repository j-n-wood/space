#include "state/faction.h"

#include <cstring>

Faction::Faction(int i, const char *n)
{
    this->id = i;
    strncpy(this->name, n, NAME_MAX_LEN - 1);
    this->name[NAME_MAX_LEN - 1] = '\0'; // Ensure null-termination
}