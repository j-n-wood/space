#pragma once

#include <cstdio>
#include "pages/base_page.h"
#include "state/shuttle.h"
#include "pages/autopilot_view.h"
#include "pages/destination_view.h"
#include "pages/drone_control_view.h"
#include "pages/page_log.h"
#include "pages/comms_view.h"

const int DroneControlViewLeft = 150;
const int DroneControlViewTop = 180;

class ShuttleView : public BasePage, EventSink
{
    Location *location;
    const TextureAsset *bodyTexture;
    const TextureAsset *itemsTexture;
    const TextureAsset *uiTexture;
    std::unique_ptr<AutopilotView> autopilotView;
    std::unique_ptr<DroneControlView> droneControlView;
    std::unique_ptr<CommsView> commsView;

    PageLog pageLog;

public:
    Craft *craft;
    DestinationPickerPtr destinationPicker;

    ShuttleView();

    void activate(ViewState &viewState) override;
    void deactivate() override;
    void input() override;
    void render() override;
    void update(const float delta) override;
    void renderDebug() override;

    // events
    void onOrbitalConstruction(Orbital *orbital) override;
};