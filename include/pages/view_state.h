#pragma once

class System;
class Location;
class Facility;

class ViewState
{
    // current UI variables
    // in theory, could have more than one, linking to viewports
    // note current location, facility to be replaced with 'focus' i.e. facility, craft, or tool page (MC) - focus is a UI concept, not part
    // of the data for craft, facilities, etc.
    System *currentSystem;
    Location *currentLocation;
    Facility *currentFacility;

public:
    ViewState() : currentSystem(nullptr), currentLocation(nullptr), currentFacility(nullptr) {};

    System *getCurrentSystem() const { return currentSystem; }
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
};