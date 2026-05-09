#pragma once

#include <cstdio>

#include "state/research_facility.h"
#include "base_page.h"

class ResearchView : public BasePage
{
    ResearchFacility *facility;

public:
    ResearchView() : facility{nullptr}
    {
        backgroundSource = pageBackgroundSources[PB_RESEARCH];
        std::snprintf(title, sizeof title, "Research");
    }
    ~ResearchView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;

    void listResearch();
};