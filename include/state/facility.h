#pragma once

#include <memory>
#include <vector>

#include "state/stores.h"

class Location;
class Factory;

class Facility
{
public:
    Location *location;
    Stores stores;
    Factory *factory; // RF bases typically do not have factory, orbitals do

    explicit Facility(Location *l) : location{l}, factory{nullptr}
    {
    }

    // advance time one tick
    virtual void update();
};

typedef std::unique_ptr<Facility> FacilityPtr;
typedef std::vector<FacilityPtr> Facilities;