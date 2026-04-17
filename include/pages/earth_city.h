#pragma once

#include <cstdio>

#include "base_page.h"

class EarthCity : public BasePage
{
public:
    EarthCity()
    {
        backgroundSource = pageBackgroundSources[PB_EARTH_CITY];
        standardButtons = ALL_STANDARD_BUTTONS | BUTTON_TRAINING | BUTTON_RESEARCH; // example, set the standard buttons for this page
        std::snprintf(title, sizeof title, "Earth City");
    }
    ~EarthCity() {}

    void input() override;
    void render() override;
};