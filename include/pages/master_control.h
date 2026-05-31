#pragma once

#include "pages/base_page.h"

class MasterControlView : public BasePage
{
    System *currentSystem;

public:
    MasterControlView();
    void activate(ViewState &viewState) override;
    void render() override;

    void renderOrbitals();
    void renderIOS();
};