#pragma once

#include "system.h"
#include <memory>

class Orrery
{
public:
    Vector2 center;
    float scale;

    System* system;
    
    ~Orrery() {
        // Note: we don't own the system pointer, so we won't free it here
    }

    void render();

    Orrery& setSystem(System* system) {
        this->system = system;
        return *this;
    }
};

typedef std::unique_ptr<Orrery> OrreryPtr;

OrreryPtr createOrrery(Vector2 center, float scale);