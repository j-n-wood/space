#pragma once

#include "pages/base_page.h"
#include "state/location.h"

class Factory;

class FactoryView : public BasePage
{
    Factory *factory;
    SublocationType sublocationType;

public:
    FactoryView(SublocationType slt) : factory{nullptr}, sublocationType{slt}
    {
        backgroundSource = pageBackgroundSources[PB_FACTORY];
        standardButtons = ALL_STANDARD_BUTTONS; // example, set the standard buttons for this page
        title = SublocationTypeName[slt] + std::string(" Factory");
    }
    ~FactoryView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;
};