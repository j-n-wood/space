#pragma once

#include <cstdio>

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
        std::snprintf(title, sizeof title, "%s Factory", SublocationTypeName[slt]);
    }
    ~FactoryView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;
};