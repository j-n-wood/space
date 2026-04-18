#include "pages/bay_view.h"
#include "state/game.h"

const char *bayTypeName[]{
    "Shuttle",
    "Space"};

void BayView::activate(ViewState &viewState)
{
    auto game{Game::getCurrent()};
    auto l{viewState.getCurrentLocation()};

    Facility *f{nullptr};
    switch (sublocationType)
    {
    case SublocationType::SLOC_ORBIT:
        f = game->orbitalAt(l);
        break;
    case SublocationType::SLOC_SURFACE:
        f = game->resourceFacilityAt(l);
        break;
    default:
        break;
    }

    if (f)
    {
        facility = f;
        std::snprintf(title, sizeof title, "%s %s Bay", SublocationTypeName[sublocationType], bayTypeName[type]);
    }
};

void BayView::input() {};

void BayView::render()
{
    BasePage::render();
};