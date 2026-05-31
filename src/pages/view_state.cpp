#include "pages/view_state.h"
#include "state/game.h"

ViewState &ViewState::setFacilityFocus(Facility *f)
{
    // not valid to use with null facility
    if (f)
    {
        setCurrentFacility(f);
        setCurrentLocation(f->location);
        setCurrentSystem(f->location->system);
        setCurrentCraft(nullptr); // clear craft focus when setting facility focus
    }
    else
    {
        TraceLog(LOG_WARNING, "Attempting to set facility focus to null, ignored");
    }
    return *this;
}

ViewState &ViewState::setCraftFocus(Craft *c)
{
    setCurrentCraft(c);
    if (c)
    {
        // set location and facility based on craft location
        setCurrentLocation(c->location);
        if (c->location)
        {
            // check for orbital first
            Game *game = Game::getCurrent();
            Facility *f = game->facilityAt(Endpoint(c->location, SLOC_ORBIT, true));
            if (!f)
            {
                f = game->facilityAt(Endpoint(c->location, SLOC_SURFACE, true));
            }
            setCurrentFacility(f);
        }
    }
    else
    {
        // if no craft, clear location and facility focus as well
        setCurrentLocation(nullptr);
        setCurrentFacility(nullptr);
    }
    return *this;
}

ViewState &ViewState::setLocationFocus(Location *l)
{
    // invalid to use with null location
    if (!l)
    {
        TraceLog(LOG_WARNING, "Attempting to set location focus to null, ignored");
        return *this;
    }
    setCurrentLocation(l);
    setCurrentSystem(l->system);
    setCurrentFacility(nullptr); // clear facility focus when setting location focus
    setCurrentCraft(nullptr);    // clear craft focus when setting location focus
    return *this;
}