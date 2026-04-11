#pragma once

#include "pages/base_page.h"

class Factory;

class FactoryView : public BasePage
{
    Factory *factory;

public:
    FactoryView() : factory{nullptr}
    {
        backgroundSource = pageBackgroundSources[PB_FACTORY];
        standardButtons = ALL_STANDARD_BUTTONS; // example, set the standard buttons for this page
        title = "Factory";
    }
    ~FactoryView() {}

    void activate() override;
    void input() override;
    void render();
};