#pragma once

#include <cstdio>
#include "pages/base_page.h"
#include "state/shuttle.h"
#include "pages/autopilot_view.h"
#include "pages/destination_view.h"

class ShuttleView : public BasePage
{
    Location *location;
    const TextureAsset *bodyTexture;
    const TextureAsset *itemsTexture;
    std::unique_ptr<AutopilotView> autopilotView;

public:
    Craft *craft;
    DestinationPickerPtr destinationPicker;

    ShuttleView()
    {
        bodyTexture = TextureManager::getInstance().getTexture(TEXTURE_BODIES);
        itemsTexture = TextureManager::getInstance().getTexture(TEXTURE_ITEMS);
        backgroundSource = pageBackgroundSources[PB_COCKPIT];
        std::snprintf(title, sizeof title, "Shuttle");
    }

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;
};