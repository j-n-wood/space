#pragma once

#include <memory>
#include <vector>

#include "state/stores.h"
#include "state/factory.h"
#include "state/waypoint.h"

class Location;

class Facility
{
public:
    int id; // database ID for loading/saving
    Location *location;
    SublocationType sublocation;
    Stores stores;
    std::unique_ptr<Factory> factory; // RF bases typically do not have factory, orbitals do

    explicit Facility(Location *l) : id{0}, location{l}, sublocation{SLOC_ORBIT}
    {
    }
    virtual ~Facility() {}

    // advance time one tick
    virtual void update();

    Factory *createFactory();
};

typedef std::unique_ptr<Facility> FacilityPtr;
typedef std::vector<FacilityPtr> Facilities;