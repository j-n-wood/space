#pragma once

#include <memory>
#include <vector>

#include "state/stores.h"

class Location;
class Factory;

class Facility
{
public:
    int id; // database ID for loading/saving
    Location *location;
    Stores stores;
    Factory *factory; // RF bases typically do not have factory, orbitals do

    explicit Facility(Location *l) : id{0}, location{l}, factory{nullptr}
    {
    }

    // advance time one tick
    virtual void update();
};

typedef std::unique_ptr<Facility> FacilityPtr;
typedef std::vector<FacilityPtr> Facilities;