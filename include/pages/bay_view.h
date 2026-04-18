#pragma once

#include <cstdio>

#include "pages/base_page.h"
#include "state/facility.h"

typedef enum
{
    BT_SHUTTLE,
    BT_SPACE,
    BT_COUNT
} BayType;

extern const char *bayTypeName[BT_COUNT];

class BayView : public BasePage
{
    SublocationType sublocationType;
    BayType type;
    Facility *facility;

public:
    BayView(SublocationType s, BayType bt) : sublocationType{s}, type{bt}
    {
        backgroundSource = pageBackgroundSources[PB_HANGAR];
        standardButtons = ALL_STANDARD_BUTTONS; // example, set the standard buttons for this page
    }
    ~BayView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;
};