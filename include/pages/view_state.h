#pragma once

class System;
class Location;
class Facility;
class Craft;
class ResearchFacility; // currently only one
class TrainingFacility; // currently only one

// What the UI is looking at. Two questions with two different answers, and both are
// derived from the focus rather than stored alongside it (architecture.md: "Location
// depends on focus"):
//
//   getCurrentPlace() -- exactly where the focus is. Names the place: "Earth Orbital",
//                        "Earth Orbit", "Earth Station".
//   getCurrentBody()  -- the body whose facilities the sidebar offers. The sidebar
//                        shows orbit-side and surface-side controls together, so it is
//                        body-scoped however precise the focus is.
//
// A UI concept only: nothing in state/ or loaders/ knows about this, and it is not
// persisted. In theory there could be more than one, linking to viewports.
class ViewState
{
    // The focus. A craft is followed, so its place is read through it rather than
    // copied -- which is why nothing has to resync when the craft moves.
    Location *focusPlace; // exactly where the focus is; null = nowhere (master control)
    Craft *focusCraft;    // the craft being followed, if any

    System *browsedSystem; // fallback when nothing is focused
    ResearchFacility *currentResearchFacility;
    TrainingFacility *currentTrainingFacility;

    int faction_id;

public:
    ViewState() : focusPlace(nullptr), focusCraft(nullptr), browsedSystem(nullptr), currentResearchFacility(nullptr), currentTrainingFacility(nullptr), faction_id(0) {};

    inline int getFactionId() const { return faction_id; }
    inline ViewState &setFactionId(int id)
    {
        faction_id = id;
        return *this;
    }

    // The focused place's system, or the one being browsed if nothing is focused.
    System *getCurrentSystem() const;
    inline ViewState &setCurrentSystem(System *s)
    {
        browsedSystem = s;
        return *this;
    }

    // Exactly where the focus is: a facility, an orbit or surface region, or a body.
    inline Location *getCurrentPlace() const { return focusCraft ? craftPlace() : focusPlace; }

    // The body whose facilities the sidebar offers. Always a celestial body, or null.
    Location *getCurrentBody() const;

    // The facility we are AT, or null if the focus is a bare region or a body. Derived,
    // so it can never disagree with the place the way a stored copy could.
    Facility *getCurrentFacility() const;

    inline Craft *getCurrentCraft() const { return focusCraft; }

    // Stop following, but stay where the craft left us.
    ViewState &setCurrentCraft(Craft *c);

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

    inline TrainingFacility *getCurrentTrainingFacility() const
    {
        return currentTrainingFacility;
    }
    inline ViewState &setCurrentTrainingFacility(TrainingFacility *tf)
    {
        currentTrainingFacility = tf;
        return *this;
    }

private:
    // Craft is an incomplete type here, so the dereference lives in the .cpp.
    Location *craftPlace() const;
};
