#pragma once

#include <cstdio>

#include "base_page.h"
#include "orrery.h"

class SystemView : public BasePage
{
    std::unique_ptr<Orrery> orrery; // owned

    // resource info
    Location *selectedLocation; // location selected in orrery, for display in resource info section
public:
    explicit SystemView() : orrery(createOrrery((Vector2){640, 400}, 1.f)), selectedLocation(nullptr)
    {
        backgroundSource = pageBackgroundSources[PB_COCKPIT];
        std::snprintf(title, sizeof title, "System View");
    }
    ~SystemView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;

    void renderLocationInfo();

    void setSystem(System *s)
    {
        orrery->setSystem(s);
    }
    void setSelectedLocation(Location *loc)
    {
        selectedLocation = loc;
    }
};