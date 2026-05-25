#pragma once

#include <cstdio>
#include "pages/base_page.h"
#include "state/shuttle.h"
#include "pages/autopilot_view.h"
#include "pages/destination_view.h"
#include "pages/page_log.h"

class ShuttleView : public BasePage
{
    Location *location;
    const TextureAsset *bodyTexture;
    const TextureAsset *itemsTexture;
    std::unique_ptr<AutopilotView> autopilotView;

    PageLog pageLog;

public:
    Craft *craft;
    DestinationPickerPtr destinationPicker;

    ShuttleView()
    {
        bodyTexture = TextureManager::getInstance().getTexture(TEXTURE_BODIES);
        itemsTexture = TextureManager::getInstance().getTexture(TEXTURE_ITEMS);
        backgroundSource = pageBackgroundSources[PB_COCKPIT];
        std::snprintf(title, sizeof title, "Shuttle");
        pageLog.top = 750;
        pageLog.left = 350;
    }

    void activate(ViewState &viewState) override;
    void deactivate() override;
    void input() override;
    void render() override;
    void update(const float delta) override;
};