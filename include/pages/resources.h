#pragma once

#include "base_page.h"

class ResourceFacility;

class Resources : public BasePage
{
    ResourceFacility *facility;

public:
    Resources() : facility{nullptr}
    {
        backgroundSource = pageBackgroundSources[PB_RESOURCES];
        standardButtons = ALL_STANDARD_BUTTONS; // example, set the standard buttons for this page
        title = "Resources";
    }
    ~Resources() {}

    void activate() override;
    void input() override;
    void render() override;

    void listResources(const Location *location);
};