#pragma once

#include "orrery.h"

class SystemView {
    Orrery* orrery;
public:
    SystemView() : orrery(nullptr) {}
    SystemView(Orrery* orrery) : orrery(orrery) {}
    ~SystemView() {}

    void render();
};