# View focus and sidebar location

> Delivered. Follows [orbit_as_location.md](orbit_as_location.md), which made
> `craft->location` precise and so split what had been one question into two.

## The problem

`ViewState` stored `currentLocation`, `currentFacility` and `currentCraft`, and the first two
answered two different questions that used to have the same answer:

1. **Which body's facilities does the sidebar offer?** `BasePage::renderStandardButtons` draws
   orbit-side *and* surface-side buttons for one body at once — two stores buttons, two shuttle
   bays, two production buttons, an orbital banner and a surface banner. It is body-scoped
   whatever the focus is doing.
2. **Where exactly is the thing I am looking at?** The header wants "Earth Orbital", "Earth
   Orbit", "Earth Station".

Once a craft's location became precise the answers diverged, and the symptoms were:

- `setFacilityFocus` stored `f->body()` while `setCraftFocus` stored `c->location`, so the two
  setters disagreed and the overlay re-pinned `currentLocation` to `craft->body()` every frame
  to compensate.
- `setCraftFocus` guessed the facility (`orbitalAt` then `resourceFacilityAt`). `orbitalAt`
  matches on `body()`, so a craft docked at Earth Station reported **Earth Orbital**. The
  overlay's copy of the same logic had no surface fallback at all.
- The header drew the body, so page titles needed "Orbital"/"Surface" prefixes to disambiguate.

[architecture.md](../../architecture.md) already specified the fix: *"Sidebar controls relate to
current location. Status bar includes focus of control. Location is nothing if focus is not at a
location. **Location depends on focus.**"*

## The model

Two stored pointers; everything else derives.

```cpp
class ViewState
{
    Location *focusPlace;  // exactly where the focus is; null = nowhere (master control)
    Craft *focusCraft;     // the craft being followed, if any
    System *browsedSystem; // fallback when nothing is focused
    ...
public:
    Location *getCurrentPlace() const { return focusCraft ? craftPlace() : focusPlace; }
    Location *getCurrentBody() const;      // place ? place->body() : nullptr
    Facility *getCurrentFacility() const;  // asFacility(place)
    System   *getCurrentSystem() const;    // place ? place->system : browsedSystem
};
```

- **Following is automatic.** The place is read *through* the craft, so it tracks as the craft
  moves. The overlay's per-frame re-pin is gone, along with its `TODO use a subscription method`.
- **The facility cannot be guessed wrong**, because it is not looked up — a docked craft's
  location *is* the facility.
- **`getCurrentLocation()` was deleted rather than renamed.** Its ten call sites split seven
  body / one place / two ambiguous. Deleting the name made the build fail at each one, which
  matters because the failure mode here is silent: `orbitalAt` returns null rather than erroring.

`setCurrentCraft(nullptr)` means "stop following, stay where you are" — it pins `focusPlace` to
the craft's place before clearing, so there is no stale window.

## Who reads which

**`getCurrentBody()`** — the sidebar ([base_page.cpp](../../src/pages/base_page.cpp)), the
stores / factory / bay pages, the resources page, the shuttle page, the console. All ask "what
does this body have". Two of them would break outright on a non-body: `location->shuttle` is
registered on the body, and `location->resources` is loaded onto bodies only.

**`getCurrentPlace()`** — the header in [overlay.cpp](../../src/pages/overlay.cpp). The
breadcrumb is `system | place | title`, and facilities are named `"<body> <label>"` with labels
"Orbital", "Station", "City", against regions named "Earth Orbit" and "Earth Surface". So the
place names its own side and the title does not have to:

```
Sol   Earth Orbital   Shuttle Bay
Sol   Earth Station   Shuttle Bay
Sol   Earth Orbit     -- in orbit, no station
```

**`getCurrentFacility()`** — the orbital page and the console: "the facility the user is looking
at". The "facility the focused craft is docked at" meaning had no reader and is gone.

## Tests

[tests/test_view_state.cpp](../../tests/test_view_state.cpp), five cases:

1. A craft docked at a **surface** station reports that station — the regression. It needs a
   body with both an orbital and a surface facility to reproduce; Earth (id 4) is one.
2. `getCurrentBody()` satisfies `locationIsBody()` for every focus kind, and the body carries
   the shuttle the sidebar looks for.
3. The place follows a craft through undock, descend and dock with no ViewState call between.
4. Dropping craft focus leaves the place where the craft was, not back at the previous focus.
5. Focused on nothing: place, body, facility and system all null, no crash.

## Not done here

- **`standardButtons` is dead.** Every page inherits `ALL_STANDARD_BUTTONS` and nothing
  overrides it; the real gating is the `enabled` switch. Its own comment asks `TODO obsolete?`.
- **`player_faction` comes only from the orbital's faction**, so on a body with a surface base
  and no orbital every surface button draws but none is clickable — `surface->faction_id` is
  never consulted. A faction bug, pre-existing.
- **`BUTTON_SELF_DESTRUCT` has no case in the `enabled` switch** so never renders, and
  `PAGE_EARTH_TRAINING` is a null page that `switchToPage` silently ignores.
- **`BayView::getSpacecraft` still compares bodies**, so it matches any IOS at that body rather
  than the one docked at this orbital. Tightening it to `ios->location == facility` is the "two
  orbitals at one body" item from [orbit_as_location.md](orbit_as_location.md).
- **The orrery never writes focus** — `SystemView` only sets its own `selectedLocation`. Wiring
  a body click to `setLocationFocus` would be additive.
