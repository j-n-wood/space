#pragma once

#include "base_page.h"
#include "orrery.h"

class SystemView : public BasePage {
    Orrery* orrery;
public:
    SystemView() : orrery(nullptr) {}
    SystemView(Orrery* o) : orrery(o) {}
    ~SystemView() {}

    void input() override;
    void render();
};