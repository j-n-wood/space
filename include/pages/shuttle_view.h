#pragma once

#include <cstdio>
#include "pages/base_page.h"
#include "state/shuttle.h"

class ShuttleView : public BasePage
{
    Location *location;
    Shuttle *shuttle;
    const TextureAsset *bodyTexture;

public:
    ShuttleView()
    {
        bodyTexture = TextureManager::getInstance().getTexture(TEXTURE_BODIES);
        backgroundSource = pageBackgroundSources[PB_COCKPIT];
        standardButtons = ALL_STANDARD_BUTTONS; // example, set the standard buttons for this page
        std::snprintf(title, sizeof title, "Shuttle");
    }

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;
};