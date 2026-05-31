#pragma once

#include "pages/base_page.h"

class MasterControlView : public BasePage
{
    int faction_id;
    System *currentSystem;

    // animation state
    // orbital production

    float orbitalAnimTime[32];

public:
    MasterControlView();
    void activate(ViewState &viewState) override;
    void render() override;
    void update(const float delta) override;

    void renderOrbitals();
    void renderIOS();
};