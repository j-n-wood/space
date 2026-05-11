#pragma once

#include <cstdio>
#include "pages/base_page.h"
#include "state/shuttle.h"
#include "pages/autopilot_view.h"

class ShuttleView : public BasePage
{
    Location *location;
    Craft *craft;
    const TextureAsset *bodyTexture;
    std::unique_ptr<AutopilotView> autopilotView;

public:
    ShuttleView()
    {
        bodyTexture = TextureManager::getInstance().getTexture(TEXTURE_BODIES);
        backgroundSource = pageBackgroundSources[PB_COCKPIT];
        std::snprintf(title, sizeof title, "Shuttle");
    }

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;
};