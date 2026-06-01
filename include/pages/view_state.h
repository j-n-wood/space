#pragma once

class System;
class Location;
class Facility;
class Craft;
class ResearchFacility; // currently only one

class ViewState
{
    // current UI variables
    // in theory, could have more than one, linking to viewports
    // note current location, facility to be replaced with 'focus' i.e. facility, craft, or tool page (MC) - focus is a UI concept, not part
    // of the data for craft, facilities, etc.
    System *currentSystem;
    Location *currentLocation;
    Facility *currentFacility;
    Craft *currentCraft;
    ResearchFacility *currentResearchFacility;

    int faction_id;

public:
    ViewState() : currentSystem(nullptr), currentLocation(nullptr), currentFacility(nullptr), currentCraft(nullptr), faction_id(0) {};

    inline int getFactionId() const { return faction_id; }
    inline ViewState &setFactionId(int id)
    {
        faction_id = id;
        return *this;
    }

    inline System *getCurrentSystem() const { return currentSystem; }
    inline ViewState &setCurrentSystem(System *s)
    {
        currentSystem = s;
        return *this;
    }

    inline Location *getCurrentLocation() const { return currentLocation; }
    inline ViewState &setCurrentLocation(Location *l)
    {
        currentLocation = l;
        return *this;
    }

    inline Facility *getCurrentFacility() const { return currentFacility; }
    inline ViewState &setCurrentFacility(Facility *f)
    {
        currentFacility = f;
        return *this;
    }

    inline Craft *getCurrentCraft() const { return currentCraft; }
    inline ViewState &setCurrentCraft(Craft *c)
    {
        currentCraft = c;
        return *this;
    }

    ViewState &setFacilityFocus(Facility *f);

    ViewState &setCraftFocus(Craft *c);

    ViewState &setLocationFocus(Location *l); // location only - part of multi-location focus

    inline ResearchFacility *getCurrentResearchFacility() const
    {
        return currentResearchFacility;
    }
    inline ViewState &setCurrentResearchFacility(ResearchFacility *rf)
    {
        currentResearchFacility = rf;
        return *this;
    }
};