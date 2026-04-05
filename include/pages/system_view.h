#pragma once

#include "base_page.h"
#include "orrery.h"

class SystemView : public BasePage {
    Orrery* orrery;
public:    
    explicit SystemView(Orrery* o) : orrery(o) {
        backgroundSource = pageBackgroundSources[PB_COCKPIT];
    }
    ~SystemView() {}

    void input() override;
    void render();
};