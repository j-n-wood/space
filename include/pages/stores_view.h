#pragma once

#include "pages/base_page.h"
#include "state/location.h"

class Stores;

class StoresView : public BasePage
{
    SublocationType sublocationType;
    Stores *stores;

public:
    StoresView(SublocationType slt) : stores{nullptr}, sublocationType(slt)
    {
        backgroundSource = pageBackgroundSources[PB_RESEARCH];
        standardButtons = ALL_STANDARD_BUTTONS; // example, set the standard buttons for this page
        title = "Stores";
    }
    ~StoresView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;

    void listResources();
};