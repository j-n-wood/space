#pragma once

#include <cstdio>

#include "pages/base_page.h"
#include "state/location.h"

class Stores;

class StoresView : public BasePage
{
    LocationType side; // LOCATION_TYPE_ORBIT or LOCATION_TYPE_SURFACE
    Stores *stores;

public:
    StoresView(LocationType s) : side{s}, stores{nullptr}
    {
        backgroundSource = pageBackgroundSources[PB_RESEARCH];
        // No orbit/surface prefix: the header reads "<system> <place> <title>" and the
        // place names its own side -- "Earth Orbital Stores", "Earth Station Stores".
        std::snprintf(title, sizeof title, "Stores");
    }
    ~StoresView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;

    void listResources();
    void listItems();
};