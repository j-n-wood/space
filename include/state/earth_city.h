#pragma once

#include "state/resourceFacility.h"

class EarthCity : public ResourceFacility
{
public:
    explicit EarthCity(Location *l);
    ~EarthCity();
};