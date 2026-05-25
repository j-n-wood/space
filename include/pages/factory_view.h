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
    SublocationType sublocationType;

    PageLog pageLog;

public:
    FactoryView(SublocationType slt) : factory{nullptr}, sublocationType{slt}
    {
        backgroundSource = pageBackgroundSources[PB_FACTORY];
        std::snprintf(title, sizeof title, "%s Factory", SublocationTypeName[slt]);

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