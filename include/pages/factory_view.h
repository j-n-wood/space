#pragma once

#include <cstdio>

#include "pages/base_page.h"
#include "state/location.h"
#include "state/event_sink.h"
#include "pages/page_log.h"

class Factory;

class FactoryView : public BasePage, EventSink
{
    Factory *factory;
    LocationType side; // LOCATION_TYPE_ORBIT or LOCATION_TYPE_SURFACE

    PageLog pageLog;

public:
    FactoryView(LocationType s) : factory{nullptr}, side{s}
    {
        backgroundSource = pageBackgroundSources[PB_FACTORY];
        // TODO orbit/surface prefix dropped -- see StoresView
        std::snprintf(title, sizeof title, "Factory");

        pageLog.top = 850;
        pageLog.left = 350;
    }
    ~FactoryView() {}

    void activate(ViewState &viewState) override;
    void deactivate() override;
    void input() override;
    void render() override;
    void update(const float delta) override;

    // events
    void onProductionComplete(Factory *factory, int item_id) override;
};