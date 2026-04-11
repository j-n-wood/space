#pragma once

#include "pages/base_page.h"

class Stores;

class StoresView : public BasePage
{
    Stores *stores;

public:
    StoresView() : stores{nullptr}
    {
        backgroundSource = pageBackgroundSources[PB_RESEARCH];
        standardButtons = ALL_STANDARD_BUTTONS; // example, set the standard buttons for this page
        title = "Stores";
    }
    ~StoresView() {}

    void activate() override;
    void input() override;
    void render();

    void listResources();
};