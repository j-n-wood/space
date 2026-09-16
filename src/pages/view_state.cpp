#include "pages/view_state.h"
#include "state/game.h"

Location *ViewState::craftPlace() const
{
    return focusCraft ? focusCraft->location : nullptr;
}

Location *ViewState::getCurrentBody() const
{
    Location *place = getCurrentPlace();
    return place ? place->body() : nullptr;
}

Facility *ViewState::getCurrentFacility() const
{
    return asFacility(getCurrentPlace());
}

System *ViewState::getCurrentSystem() const
{
    Location *place = getCurrentPlace();
    return (place && place->system) ? place->system : browsedSystem;
}

ViewState &ViewState::setCurrentCraft(Craft *c)
{
    if (!c && focusCraft)
    {
        // Stop following, stay put. Without this the place would fall back to whatever
        // was focused before the craft was picked up.
        focusPlace = focusCraft->location;
    }
    focusCraft = c;
    return *this;
}

ViewState &ViewState::setFacilityFocus(Facility *f)
{
    // not valid to use with null facility
    if (f)
    {
        focusCraft = nullptr;
        focusPlace = f; // a facility IS a place; the body follows from body()
    }
    else
    {
        TraceLog(LOG_WARNING, "Attempting to set facility focus to null, ignored");
    }
    return *this;
}

ViewState &ViewState::setCraftFocus(Craft *c)
{
    if (c)
    {
        // The craft's own location is exact -- docked, it IS the facility. Nothing is
        // looked up, so there is nothing to guess wrong.
        focusCraft = c;
        focusPlace = c->location;
    }
    else
    {
        setCurrentCraft(nullptr);
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
    focusCraft = nullptr;
    focusPlace = l;
    browsedSystem = l->system;
    return *this;
}
