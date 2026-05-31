#include "pages/orbital_view.h"
#include "state/orbital.h"
#include <cstdio>

OrbitalView::OrbitalView()
{
    backgroundSource = pageBackgroundSources[PB_ORBITAL];
    std::snprintf(title, sizeof title, "Orbital");
}

void OrbitalView::activate(ViewState &viewState)
{
    if (viewState.getCurrentFacility() && viewState.getCurrentFacility()->sublocation == SLOC_ORBIT)
    {
        orbital = dynamic_cast<Orbital *>(viewState.getCurrentFacility());
    }
    else
    {
        orbital = nullptr;
    }
}

void OrbitalView::render()
{
    // render background and standard buttons as normal
    BasePage::render();

    // TODO render orbital contents - e.g. factory progress, resources in transit, etc.
}

void OrbitalView::update(const float delta)
{
    // nothing to update for now - orbital contents are static, but could add animations or dynamic elements later
}