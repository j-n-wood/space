#pragma once

#include <memory>
#include <vector>

#include "state/stores.h"
#include "state/factory.h"
#include "state/waypoint.h"
#include "state/location.h"

class Facility
{
public:
    int id; // database ID for loading/saving
    int faction_id;

    // What this facility IS. Shares LocationType with bodies because a facility is a
    // kind of place. Set at construction; only an explicit upgrade should ever change
    // it. Independent of `sublocation`, which says where it sits -- so a future kind
    // can exist at either surface or orbit without inventing a type per placement.
    LocationType type;

    Location *location;
    SublocationType sublocation;
    Stores stores;
    std::unique_ptr<Factory> factory; // RF bases typically do not have factory, orbitals do
    bool operational;                 // fully constructed
    bool aoc_installed;
    bool sdm_installed;
    bool mtx_installed;
    uint8_t construction_progress; // 0-100, for construction progress of facility, if under construction
    float damage;                  // 0-100, for damage level of facility, if damaged

    Facility(Location *l, LocationType t, SublocationType s) : id{0}, faction_id{0}, type{t}, location{l}, sublocation{s}, operational{false}, aoc_installed{false}, sdm_installed{false}, mtx_installed{false}, construction_progress{0}, damage{0}
    {
    }
    virtual ~Facility() {}

    // advance time one tick
    virtual void update();

    Factory *createFactory();
};

typedef std::unique_ptr<Facility> FacilityPtr;
typedef std::vector<FacilityPtr> Facilities;