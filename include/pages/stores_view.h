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
        // TODO the header reads "<system> <location> <title>", and the location name
        // already says which side ("Earth Orbital", "Earth Station"), so the prefix is
        // redundant -- once the header shows the precise location rather than the body.
        std::snprintf(title, sizeof title, "Stores");
    }
    ~StoresView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;

    void listResources();
    void listItems();
};