# Architecture

## Make it work first

## Separate simulation from presentation

Ideally want data simulation to work independent of UI. Could replace UI
entirely or run headless in a separate process.

## Data driven UI

* reloadable

### Actions

* e.g. make 'switch to page' an action s.t. can drive from console with same code

Long term - suitable for e.g. offline/multiplayer, or recording 'demo' mode for
debugging.

## Console for input

Test stuff, run saved journal, etc.

## Classes

### Data

#### Craft

Ships, shuttles. Are they the same? Either can be focus and they work much the way.
They can also be at a location (or in transit). Therefore location can be derived
from craft. This affects visuals and controls.

Assume we make them the same. There can be different collections if that suits.

Decide if AOS or SOA is better for processing. SOA is more efficient but probably
overkill unless we have 1000s. Or we can relate struct data to SOA for 'positions'
etc via an index. Start with structs and we can refactor if needed.

e.g.

```c++

class SystemCoordinate {
public:
    float   radius;
    float   angle;
};

class Craft {
public:
    std::string name;
    uint8_t     fuel;
    Location*   location;   // where you at right now?
    CraftState  state;      // what you doing?
    uint32_t    state_time;    // state transition time e.g. ascent, transit, work

    // IP capable can have polar coordinates
    SystemCoordinate    coordinate;
};
```

#### Locations

* bodies like planets, moons, asteriod belt, Earth city. Also stars for interstellar travel target.
* have UI that depends on state at the location
* ships could be a location? Nah, you can't navigate to a ship. Make something for purpose.
* shuttle is a 'UI facility' at a location, if shuttle is present.
* game state can have a 'current location'.

Sub-locations can be handled as a state with a transition time.

Enum the lot - then every time we add a state, have to alter enum.
Many duplicates for surface/orbit for shuttles - matters? Makes it easier
to track shuttle ascent/descent.

```c++
enum {
    CS_SURFACE,  // surface no dock
    CS_SURFACE_DOCKED,
    CS_SURFACE_WORK,
    CS_SURFACE_LAUNCH, // transient state leaving dock
    CS_ASCENDING,
    CS_ORBIT,
    CS_ORBIT_DOCKING, // transient state entering dock
    CS_ORBIT_DOCKED,
    CS_ORBIT_WORK,
    CS_ORBIT_LAUNCH, // transient state leaving dock
    CS_DESCENDING,
    CS_TRANSIT, // IP or IS transit - refine with type and speed
    CS_COUNT
} CraftState;

class Location {
public:
    System*         system; // needed?
    std::string     name;
    LocationType    type;   // star, planet, moon, asteroids...
};
```

Or make state classes, more powerful but overkill?

```c++
class CraftState {
public:
    const char* name;
    
    // don't really want to have UI elements referenced here, suggests enum approach
};
```

Currently each location is a body and can have orbital, surface facilities,
EC facilities. Only one orbital or SF, but could allow for more and more
types later. We don't need to process/update locations - just OF and SF.

Therefore have collections of those to update, which have reference to location.

EC as special case or SF with bonuses. It's the only SF with production. Treat
as 'magic orbital' or have update list of 'factories' and EC is a magic factory?

```c++
class Factory {
public:
    Location* location; // so don't have to process from location down, can sent output to correct stores. Note need to distinguish orbit from surface for EC. Or give reference to a 'stores' object, which can provide resource and be production target.

    // what are we building
    // how much progress, required progress
    // queue of things to build
};
```

Also note that orphan SF can exist - the broken moon base, or if you recapture
an orbital - unclear what happens if an orbital is destroyed.

### UI

Pages/page manager - render for some facility type.
Depends on current system, location.

Determination of what UI controls are enabled is based on current location.

* surface stores, shuttle bay, resources depend on having a surface base
* training, research are only at EC
* orbit production, stores, shuttle and space bay depend on OF presence
* shuttle is available if there is a shuttle and one of OF or RF resource facility
* self destruct is present if OF has it

Controls depend on location not page. If location is a ship, then none 
of those controls are relevant. Suggests ship as a location, but not a 
navigation target? Or keep data more separate and have UI use some other
concept to show what the 'focus' is?

Original has upper status with system name, lower status is time then location.
Tooltip as upper left status.

Location is earth city until you get earth orbital, as far as controls are concerned. At master control, controls vanish. Same at asteroids.

Location is earth city, earth shuttle, earth orbital, IOS-001, or nothing like
master control (nowhere).

When location is earth shuttle, controls change based on where you 'get off' the
shuttle - if you go to shuttle bay at EC, location changes to EC.

On ship, location is ship. Arrive at asteroids - still ship.

Therefore we have location and focus are UI concepts. Both are valid for craft.

Sidebar controls relate to current location. Status bar includes focus of control.

Focus can be:

* nothing (MC)
* EC
* a shuttle
* a ship
* an orbital

Location can be:

* nothing
* EC
* any body (star, planet, moon, asteroids)

Location is nothing if focus is not at a location. Location depends on focus.
Focus is a UI state thing. It relates to data objects e.g. craft.

#### Craft UI

Can map state enums to icons.