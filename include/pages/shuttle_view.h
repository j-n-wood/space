#pragma once

#include <cstdio>
#include "pages/base_page.h"
#include "state/shuttle.h"
#include "pages/autopilot_view.h"
#include "pages/destination_view.h"
#include "pages/drone_control_view.h"
#include "pages/page_log.h"

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

    PageLog pageLog;

public:
    Craft *craft;
    DestinationPickerPtr destinationPicker;

    ShuttleView()
    {
        bodyTexture = TextureManager::getInstance().getTexture(TEXTURE_BODIES);
        itemsTexture = TextureManager::getInstance().getTexture(TEXTURE_ITEMS);
        uiTexture = TextureManager::getInstance().getTexture(TEXTURE_UI);
        backgroundSource = pageBackgroundSources[PB_COCKPIT];
        std::snprintf(title, sizeof title, "Shuttle");
        pageLog.top = 750;
        pageLog.left = 350;
        droneControlView = std::make_unique<DroneControlView>(DroneControlViewLeft, DroneControlViewTop);
    }

    void activate(ViewState &viewState) override;
    void deactivate() override;
    void input() override;
    void render() override;
    void update(const float delta) override;
    void renderDebug() override;

    // events
    void onOrbitalConstruction(Orbital *orbital) override;
};