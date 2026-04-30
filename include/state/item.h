#pragma once

#include <vector>

#include "state/resources.h"
#include "state/string_caps.h"
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

// order matches database table ID
enum ItemType
{
    Derrick,
    S_Chassis,
    S_Drive,
    I_Chassis,
    I_Drive,
    Of_Frame,
    Supply_Pod,
    Tool_Pod,
    Cryo_Pod,
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
    Pulse_Blaster_Laser
};

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
    char name[NAME_MAX_LEN];
    char description[DESC_MAX_LEN];
    bool tool;       // can tool pod
    bool researched; // can produce
    int tech_level;  // required to produce
    bool orbital;    // produced in orbit only
    int mass;        // descriptive
    float research_time;
    float research_remaining;
    float production_time;
    int doc_image_index;
    int production_image_index;
    int pod_capacity; // how many units can fit in a pod
    std::vector<BuildRequirement> requirements;
};