#pragma once

#include <string>
#include <vector>

#include "state/resources.h"
/*
enum ItemType
{
    Derrick,
    S_Chassis,
    S_Drive,
    Of_Frame,
    Supply_Pod,
    Tool_Pod,
    Cryo_Pod,
    I_Chassis,
    I_Drive,
    ACC,
    AOC,
    Bandaid,
    SDM,
    Grapple,
    DFCC,
    AMA,
    Hyperlight,
    MTX,
    MFL,
    R_Frame,
    Prejudice_Torpedo_Launcher,
    Commspod,
    Ios_Drone,
    G_Chassis,
    Star_Drive,
    PTL,
    Star_Drone,
    Prison_Pod,
    Sonic_Blaster,
    Pulse_Blaster_Laser,
    Count
};

extern const char *ItemName[ItemType::Count];
*/

class BuildRequirement
{
public:
    ResourceType resource;
    int amount;

    explicit BuildRequirement(ResourceType rt, int am) : resource{rt}, amount{am} {};
};

// Item definitions
class Item
{
public:
    int id;
    std::string name;
    std::string description;
    bool tool;       // can tool pod
    bool researched; // can produce
    int tech_level;  // required to produce
    bool orbital;    // produced in orbit only
    int mass;        // descriptive
    int research_time;
    int research_progress;
    int production_time;
    std::vector<BuildRequirement> requirements;
};