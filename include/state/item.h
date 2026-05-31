#pragma once

#include <vector>

#include "state/resources.h"
#include "state/string_caps.h"

// order matches database table ID
enum ItemType
{
    None, // no item
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
    Star_Drone,
    Prison_Pod,
    Sonic_Blaster,
    Pulse_Blaster_Laser,
    MAX_ITEM_TYPE
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
    int pod_type;    // PodType this loads onto; PT_EMPTY for non-pod items
    bool researched; // can produce
    int tech_level;  // required to produce
    bool orbital;    // produced in orbit only
    int mass;        // descriptive
    float production_time;
    int doc_image_index;
    int production_image_index;
    int pod_capacity; // how many units can fit in a pod
    std::vector<BuildRequirement> requirements;

    Item() : id{0}, name{""}, description{""}, pod_type{0}, researched{false}, tech_level{0}, orbital{false}, mass{0}, production_time{0}, doc_image_index{-1}, production_image_index{-1}, pod_capacity{0} {}
};