#pragma once

#include <cstdio>

#include "base_page.h"

class ResourceFacility;

class Resources : public BasePage
{
    ResourceFacility *facility;
    Location *location;

public:
    Resources() : facility{nullptr}
    {
        backgroundSource = pageBackgroundSources[PB_RESOURCES];
        std::snprintf(title, sizeof title, "Resources");
    }
    ~Resources() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;

    void listResources();
};