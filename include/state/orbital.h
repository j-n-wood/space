#pragma once

#include "state/facility.h"
#include "state/factory.h"

class Orbital : public Facility
{

public:
    explicit Orbital(Location *l);

    virtual void update() override;
};

typedef std::unique_ptr<Orbital> OrbitalPtr;
typedef std::vector<OrbitalPtr> Orbitals;