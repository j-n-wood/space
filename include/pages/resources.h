#pragma once

#include "base_page.h"

class Location;

class Resources : public BasePage
{
public:
    Resources()
    {
        backgroundSource = pageBackgroundSources[PB_RESOURCES];
        standardButtons = ALL_STANDARD_BUTTONS; // example, set the standard buttons for this page
        title = "Resources";
    }
    ~Resources() {}

    void input() override;
    void render();

    void listResources(const Location *location);
};