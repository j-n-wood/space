#pragma once

#include "state/resourceFacility.h"

class EarthCity : public ResourceFacility
{
public:
    int population;

    explicit EarthCity(Location *l);
    ~EarthCity();
};