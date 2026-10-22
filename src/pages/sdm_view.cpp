#include "pages/sdm_view.h"
#include "state/facility.h"

SDMView::SDMView()
{
    facility = nullptr;
}

void SDMView::activate(ViewState &viewState)
{
    facility = viewState.getCurrentFacility();
}

void SDMView::deactivate()
{
    facility = nullptr;
}

void SDMView::input()
{
}

void SDMView::render()
{
    BasePage::render();

    if (facility)
    {
        if (facility->sdm_installed)
        {
            if (facility->sdm_active)
            {
                DrawText("SDM Active", 200, 60, 30, RED);
            }
            else
            {
                DrawText("SDM Inactive", 200, 60, 30, GRAY);
            }
        }
    }
}

void SDMView::update(const float delta)
{
}

void SDMView::onFacilityDestroyed(Facility *f)
{
    if (facility == f)
    {
        facility = nullptr;

        // boom!
    }
}