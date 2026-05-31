#pragma once

#include "pages/base_page.h"

class Orbital;

class OrbitalView : public BasePage
{
    Orbital *orbital;

public:
    OrbitalView();
    void activate(ViewState &viewState) override;
    void render() override;
    void update(const float delta) override;
};