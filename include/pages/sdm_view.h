#pragma once

#include "pages/base_page.h"
#include "state/event_sink.h"

class Facility;

class SDMView : public BasePage, EventSink
{
    Facility *facility;

public:
    SDMView();

    void activate(ViewState &viewState) override;
    void deactivate() override;
    void input() override;
    void render() override;
    void update(const float delta) override;

    void onFacilityDestroyed(Facility *f) override;
};