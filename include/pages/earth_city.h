#pragma once

#include <cstdio>

#include "base_page.h"

class EarthCityView : public BasePage
{
public:
    EarthCityView()
    {
        backgroundSource = pageBackgroundSources[PB_EARTH_CITY];
        std::snprintf(title, sizeof title, "Earth City");
    }
    ~EarthCityView() {}

    void input() override;
    void render() override;
};