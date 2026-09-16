#pragma once

#include <cstdio>

#include "base_page.h"

class EarthCityView : public BasePage
{
public:
    EarthCityView()
    {
        backgroundSource = pageBackgroundSources[PB_EARTH_CITY];
        // No title: the place is already named "Earth City" in the header.
        title[0] = '\0';
    }
    ~EarthCityView() {}

    void input() override;
    void render() override;
};