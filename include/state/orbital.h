#pragma once

#include "state/facility.h"
#include "state/factory.h"

class Orbital : public Facility
{

public:
    explicit Orbital(Location *l, SublocationType s = SLOC_ORBIT);

    virtual void update() override;
};

// Non-owning: Game::locations owns every location, facilities included.
typedef std::vector<Orbital *> Orbitals;