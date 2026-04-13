#pragma once

enum ResourceType
{
    Unknown = 0,
    Iron = 1,
    Titanium = 2,
    Aluminium = 3,
    Carbon = 4,
    Copper = 5,
    Hydrogen = 6,
    Deuterium = 7,
    Methane = 8,
    Helium = 9,
    Palladium = 10,
    Platinum = 11,
    Silver = 12,
    Gold = 13,
    Silica = 14,
    MehFuel = 15,
    HedFuel = 16,
    Count = 17
};

extern const char *ResourceName[ResourceType::Count];