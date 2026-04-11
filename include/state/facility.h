#pragma once

#include <memory>
#include <vector>

#include "state/stores.h"

class Facility
{
public:
    Stores stores;

    // advance time one tick
    virtual void update();
};

typedef std::unique_ptr<Facility> FacilityPtr;
typedef std::vector<FacilityPtr> Facilities;