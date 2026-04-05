#pragma once

#include "base_page.h"

class EarthCity : public BasePage {    
public:
    EarthCity() {
        backgroundSource =  pageBackgroundSources[PB_EARTH_CITY];
    }
    ~EarthCity() {}

    void input() override;
    void render();
};