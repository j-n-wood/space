# Facilities as locations


## Context

Facilities and celestial bodies are two spatial models that do not meet. A body is a
`Location` with a position, a parent and children; a facility is a separate object that
merely *points at* a `Location`. Everything that wants to travel to a facility has to cross
the gap, and the crossing is `Endpoint{location, sublocation, docked}` plus
`Game::facilityAt()` to turn the triple back into a `Facility*`.

Making a facility a kind of `Location` collapses that. A travel endpoint becomes a target
location plus a desired arrival state; `facilityAt` disappears; multiple facilities per body
become ordinary children rather than a special case; and facilities acquire real positions,
so transit time and eventual orrery rendering come from the machinery bodies already use.

**Decisions taken:** `Facility` becomes a subclass of `Location`; the `System` parallel-array
dissolution lands as a separate prior stage; facility-locations are invisible in the orrery
initially (radius 0, exactly as the asteroid belt and `space` bodies already behave).

**Relationship to the other plans.** There are two distinct facility lookups, and this change
affects them differently:

- *"Which facility is at this location"* — what
  [`docs/plans/facility_lookup.md`](facility_lookup.md) addresses. Its conclusion
  survives, but the hierarchy supplies the mechanism: the per-location collection it proposes
  becomes `Location::children`, and the scan is over the same 1–4 entries. That plan is
  subsumed rather than wrong — do not implement it separately.
- *"Which facilities are near some location"* — proximity and ranking, for faction AI picking
  targets. **Does not exist yet**, and is not addressed here. Worth noting this change
  *enables* it: once facilities are locations they have resolved positions, so the query is a
  distance sort over `allOrbitals()` rather than a facility→location→position hop. Re-assess
  when the need is real.

[`docs/plans/facility_type.md`](facility_type.md) remains a prerequisite and should
land first, but **needs one adjustment**: its discriminator should be the extended
`LocationType` (Stage 2) rather than a new `FacilityType` enum. Its numbering already matches
— `FT_RESOURCE/FT_ORBITAL/FT_EARTH_CITY` become `LocationType` values — so the field Stage 2
inherits is literally the same field, with no churn. Its other two halves are unchanged and
still needed: dropping `SLOC_EARTH_CITY` from `SublocationType`, and replacing `saveBase`'s
`training_facility || research_facility` inference with a stored value.

### What makes this viable

- `Location::children` already exists, is built from `primary_id`
  ([load_system.cpp:177](../../src/loaders/load_system.cpp#L177)), and has **zero UI consumers** —
  only `main.cpp:79` and two test assertions read it. Adding facilities breaks nothing.
- Transit time is already `getResolvedPosition(from) → getResolvedPosition(to)` over any two
  `Location*` ([game.cpp:16-30](../../src/state/game.cpp#L16-L30)), so facility-locations get
  travel times and angular separation for free.
- There is **no `switch` on `LocationType` anywhere** — four consumers total, all in
  `load_system.cpp` and one orbit-ring test in `orrery.cpp`. New types cost nothing.
- `facilityAt` ([game.cpp:128-139](../../src/state/game.cpp#L128-L139)) is the only code that
  consumes the triple as a triple, and its whole job is to undo it.

### What blocks it

`System` holds body physics in **seven parallel `std::vector`s indexed by `Location::index`**
([system.h:29-35](../../include/state/system.h#L29-L35)), sized once by a destructive
`setNumBodies()` ([system.cpp:12-23](../../src/state/system.cpp#L12-L23)) with no grow path. Plus:

- `Orrery::body_positions[64]` ([orrery.h:12](../../include/orrery.h#L12)) is fixed and written
  unchecked; `MAX_BODY_COUNT` is declared and never used. Sol has 38 bodies and 13
  facilities today.
- `Game::locationByID` is `locations[id]`, a positional index
  ([game.cpp:153-160](../../src/state/game.cpp#L153-L160)), so ids must stay dense and 0-based.
- **Save/load compounds.** `saveSystem` loops `system->locations.size()`, not `numPlanets`,
  writing every entry to `bodies`; `countSystemBodies` re-counts them next load, inflating
  `numPlanets` *every cycle*.

Stage 1 removes all of these.

---

## Stage 1 — dissolve `System`'s parallel arrays onto `Location`

Independently valuable: it deletes the `index` alignment invariant, the 64-body ceiling, the
save/load inflation, and several uninitialised-field bugs. Land and test it alone.

`Location` absorbs the orbital elements and gains a parent pointer:

```cpp
class Location
{
public:
    char name[NAME_MAX_LEN];
    LocationType type;
    System *system{nullptr};
    int id{-1};

    Location *primary{nullptr};   // replaces primary_id lookups + planetPrimaryIndexes
    int primary_id{-1};           // persistence only; resolved to `primary` on load
    std::vector<Location *> children;

    // orbital elements -- were System's parallel arrays, indexed by `index`
    float orbital_radius{0.0f};
    float orbital_velocity{0.0f};
    float initial_angle{0.0f};
    float radius{0.0f};           // display radius; 0 = not drawn, not hit-testable
    Color color{WHITE};
    Vector2 position{0.0f, 0.0f}; // local position about `primary`, refreshed per tick

    LocationResources resources;
    ShuttlePtr shuttle;

    Vector2 resolvedPosition() const;
};
```

`index` and `system_id` are **deleted** — `system_id` is `system->id`, and nothing needs a
positional index once the arrays are gone. This removes the uninitialised-`index` hazard for
runtime-created locations ([location.cpp:9-12](../../src/state/location.cpp#L9-L12)).

```cpp
// src/state/system.cpp
void System::update(float time)
{
    for (Location *l : locations)
    {
        const float angle = time * l->orbital_velocity + l->initial_angle;
        l->position = {l->orbital_radius * cosf(angle), l->orbital_radius * sinf(angle)};
    }
}

// src/state/location.cpp -- iterative, so a malformed parent chain cannot blow the stack
Vector2 Location::resolvedPosition() const
{
    Vector2 p = position;
    int guard = 0;
    for (const Location *n = primary; n && guard < 16; n = n->primary, ++guard)
    {
        p.x += n->position.x;
        p.y += n->position.y;
    }
    return p;
}
```

`System::getResolvedPosition`, `setNumBodies`, `numPlanets` and all seven vectors go.
`System::primary` and `space` get `nullptr` initialisers they currently lack
([system.cpp:7-10](../../src/state/system.cpp#L7-L10)).

**`Orrery` loses its cache entirely.** `body_positions`, `MAX_BODY_COUNT`, `updatePositions`
and `last_time` are deleted; `renderPosition` takes a `Location*` and calls
`resolvedPosition()` (a handful of adds, depth ≤ 3). All four `numPlanets` loops become
`for (Location *l : system->locations)`. `focus_index` becomes `Location *focus_location`.
The transit-draw path ([orrery.cpp:143-160](../../src/orrery.cpp#L143-L160)) stops indexing
`->index` into a fixed array — the current unchecked, cross-system-unsafe read.

**`Game::locationByID` becomes a real lookup.** Ids no longer need to be dense, which Stage 2
depends on. `Loader::findLocation` ([loader.cpp:66-81](../../src/loaders/loader.cpp#L66-L81))
already scans correctly and is the model; a scan over ~180 entries called only from the
loader and scaffolding is fine.

**Loader/saver** read and write the fields directly. `load_system.cpp`'s pre-size pass and
`systemIndex[10]` go; the hierarchy fix-up sets `loc->primary` instead of
`planetPrimaryIndexes`. Fix the `break` that should be `continue`
([load_system.cpp:158](../../src/loaders/load_system.cpp#L158)) — it currently aborts the fix-up at
body 164, leaving `System::space` uninitialised for systems 2-9, which
[craft.cpp:236](../../src/state/craft.cpp#L236) then dereferences. `saveLocation` reads
`location->orbital_radius` rather than `planetDistances[locationIndex]`, which is what kills
the compounding bug.

## Stage 2 — `Facility : public Location`

Ownership consolidates: `Game::locations` owns every location including facilities;
`bases` and `orbitals` become non-owning `std::vector<ResourceFacility*>` /
`std::vector<Orbital*>` for iteration — matching how `shuttles`, `factories` and
`researchFacilities` already work. `Location` gains a virtual destructor.

```cpp
class Facility : public Location
{
    FacilityType facility_type;      // from facility_type.md
public:
    SublocationType sublocation;     // surface vs orbit: what actions a craft may take here
    int faction_id;
    Stores stores;
    std::unique_ptr<Factory> factory;
    bool operational;
    ...
    FacilityType type() const { return facility_type; }
};
```

Note `Location::type` (a `LocationType`) and `Facility::type()` (a `FacilityType`) coexist —
worth renaming the accessor `facilityType()` to avoid the shadowing trap.

**`LocationType` is the discriminator** — extended, not supplemented by a parallel enum. New
values are *appended*, since 0-5 are persisted in `bodies.type`:

```cpp
enum LocationType
{
    LOCATION_TYPE_STAR = 0,
    LOCATION_TYPE_PLANET,
    LOCATION_TYPE_MOON,
    LOCATION_TYPE_ASTEROID_BELT,
    LOCATION_TYPE_EARTH_CITY,        // already exists -- zero rows, zero readers today
    LOCATION_TYPE_SPACE,
    LOCATION_TYPE_ORBITAL,           // new
    LOCATION_TYPE_RESOURCE_FACILITY, // new
    LOCATION_TYPE_MAX
};

inline bool locationIsFacility(LocationType t)
{
    return t == LOCATION_TYPE_ORBITAL
        || t == LOCATION_TYPE_RESOURCE_FACILITY
        || t == LOCATION_TYPE_EARTH_CITY;
}

// Facility* from a Location*, or nullptr. No RTTI -- the enum is the discriminator.
inline Facility *asFacility(Location *l)
{
    return (l && locationIsFacility(l->type)) ? static_cast<Facility *>(l) : nullptr;
}
```

`LOCATION_TYPE_EARTH_CITY` already exists and is dead — no `bodies` row uses it and no code
reads it — which reads as the original intent for exactly this. It now earns its place.

**This absorbs `FacilityType`.** The three facility kinds are `LocationType` values, so the
separate enum proposed in `facility_type.md` is not needed: `Location::type` *is* the facility
type, and `Facility::sublocation` remains the independent surface/orbit axis, as decided
there. See the sequencing note below.

Since no `switch` on `LocationType` exists anywhere, nothing breaks implicitly — and equally
nothing warns. This is the moment to make it an `enum class` and let `-Wswitch` police it, per
the pattern in [`docs/plans/craft_state.md`](craft_state.md) §2; the new values
make that pay off immediately.

The factory methods set up the parent link and the identity:

```cpp
Orbital *Game::createOrbital(Location *parent)
{
    auto o = std::make_unique<Orbital>(parent);   // sets type, sublocation, radius 0
    Orbital *ptr = o.get();
    ptr->id = ++location_max_id;                  // seeded past the max body id on load
    ptr->system = parent->system;
    ptr->primary = parent;
    ptr->orbital_radius = ORBITAL_STATION_RADIUS; // low orbit; angle distinguishes siblings
    parent->children.push_back(ptr);
    locations.push_back(std::move(o));
    orbitals.push_back(ptr);
    createFactory(ptr);
    return ptr;
}
```

`radius` stays 0 so the orrery neither draws nor hit-tests it — the same treatment the
asteroid belt and `space` bodies already get. `orbital_radius` and `initial_angle` are set
anyway, so transit times are right and the later rendering work is a one-line radius change.

**Persistence.** Facilities keep their own `facilities` table but gain the location columns
they need (`name`, `primary_id`, `orbital_radius`, `orbital_velocity`, `initial_angle`).
They are *not* written to `bodies`, so `bodies` stays a pure celestial-body table and the
loader order is: bodies → locations → facilities as child locations.

**Id allocation — already in place.** Because Stage 1 made `locationByID` a lookup, facility
ids only need to be unique, not dense. `Game::location_max_id` and `nextLocationID()` landed
with the facility-type work; facilities draw from them to extend the loaded body sequence
rather than colliding with it. This also supplies the stable facility identity that
`facility_lookup.md` flagged as missing.

Two details worth keeping:

- The watermark is **derived, never persisted** — a stored counter can drift from the data it
  describes. `craft_max_id` sets the precedent, rebuilt during load by
  [loader.cpp:418-420](../../src/loaders/loader.cpp#L418-L420).
- It is maintained in `Game::createLocation`, not in the loader, so *every* creation path
  updates it. The craft equivalent only updates during load, so a craft created with an
  explicit id at runtime would not move the mark — safe today only because every runtime
  craft goes through `++craft_max_id`.

## Stage 3 — collapse `Endpoint`

```cpp
class Endpoint
{
public:
    Location *location{nullptr};        // a body, or a facility-location
    CraftState desired_state{CS_ORBIT}; // one of the four stable states
};
```

`sublocation` and `docked` both go: docking is `desired_state == CS_ORBIT_DOCKED` or
`CS_SURFACE_DOCKED`, and where a craft ends up is a property of the target. `atEndpoint()` —
today the load-bearing bridge between the decomposed Endpoint and the overloaded
`CraftState`, and named as the root mismatch in `craft_state.md` — becomes:

```cpp
inline bool atEndpoint() const
{
    const Endpoint &d = currentDestination();
    return d.location == location && state == d.desired_state;
}
```

`Game::facilityAt(const Endpoint&)` is **deleted**; callers that want the facility at a
target use `asFacility(endpoint.location)` — an enum test and a `static_cast`, no RTTI.
`Autopilot::onDocked`
([autopilot.cpp:51-52](../../src/state/autopilot.cpp#L51-L52)) becomes two casts instead of two
scans, and its "source/dest orbital destroyed" TODO becomes a null check that means
something. The throwaway Endpoints built purely as lookup keys in
[view_state.cpp:32-35](../../src/pages/view_state.cpp#L32-L35) and
[overlay.cpp:78](../../src/pages/overlay.cpp#L78) disappear.

`craft_destinations` drops `sublocation`/`docked` for `desired_state`;
`MAX_DESTINATIONS = 2` is untouched, and the missing bounds check on `destIndex`
([loader.cpp:449-459](../../src/loaders/loader.cpp#L449-L459)) is worth adding while there.

## Files

**Stage 1:** [include/state/location.h](../../include/state/location.h),
[src/state/location.cpp](../../src/state/location.cpp),
[include/state/system.h](../../include/state/system.h),
[src/state/system.cpp](../../src/state/system.cpp), [include/orrery.h](../../include/orrery.h),
[src/orrery.cpp](../../src/orrery.cpp),
[src/loaders/load_system.cpp](../../src/loaders/load_system.cpp),
[src/loaders/save_game.cpp](../../src/loaders/save_game.cpp) (`saveLocation`),
[src/state/game.cpp](../../src/state/game.cpp) (`locationByID`, `createLocation`,
`calculateTransitTime`), [tests/test_db.cpp](../../tests/test_db.cpp) (the `numPlanets` assertions
at :74-95).

**Stage 2:** [include/state/facility.h](../../include/state/facility.h) and the three facility
subclasses, [include/state/game.h](../../include/state/game.h) + `game.cpp` (ownership, factories,
`location_max_id`), [src/loaders/loader.cpp](../../src/loaders/loader.cpp) +
[save_game.cpp](../../src/loaders/save_game.cpp) (facility schema), `resources/initial.db`.

**Stage 3:** [include/state/waypoint.h](../../include/state/waypoint.h),
[include/state/craft.h](../../include/state/craft.h) + `craft.cpp`,
[src/state/autopilot.cpp](../../src/state/autopilot.cpp),
[src/pages/shuttle_view.cpp](../../src/pages/shuttle_view.cpp),
[src/pages/view_state.cpp](../../src/pages/view_state.cpp),
[src/pages/overlay.cpp](../../src/pages/overlay.cpp), both loaders, `craft_destinations` schema.

## Verification

Each stage must build and pass `make && make tests && ./bin/Debug/tests` before the next
begins. A new test file needs `./reconf.sh`.

**Stage 1 — positions are unchanged.** This is a pure representation change, so the test is
equivalence: for every location in `initial.db`, `resolvedPosition()` at a fixed `game_time`
must equal what `getResolvedPosition(index)` produced before. Capture the current values
first (a throwaway test printing all 173) and assert against them afterwards. Then:
`calculateTransitTime(Earth, Mars)` unchanged at a fixed time; save/load round-trip leaves
every orbital element identical; **`numPlanets` no longer inflates across repeated
save→load→save cycles** (run three cycles and assert the body count is stable — this fails
on today's code once `locations.size()` exceeds `numPlanets`); `System::space` is non-null
for *every* system, including 2-9, which it is not today.

**Stage 2 — facilities are locations.** An orbital's `primary` is its body and it appears in
that body's `children`; `resolvedPosition()` is near the parent's; `radius == 0` so
`mouseOverBodyIndex()` never returns it and the orrery draws nothing new (compare a rendered
frame's body count before and after). Save/load round-trips facilities with their parent link
and orbital elements. Two orbitals at one body get distinct `initial_angle` and distinct
positions. The existing `test_db.cpp` facility round-trip cases must pass unchanged.

**Stage 3 — travel still works.** `atEndpoint()` agrees with the old triple logic across all
four stable states; an autopilot shuttle completes a full surface↔orbit cargo cycle; an IOS
completes a transit between bodies and arrives docked; `craft_destinations` round-trips.

**Play-test after each stage:** `./bin/Debug/space` — system view renders Sol correctly
with moons in the right places, the destination picker still selects bodies, F5/F8
round-trips, and a craft in transit draws on the orrery line between its endpoints.

## Risks

- **Stage 1 is a wide, mechanical diff** across state, loaders and rendering with no
  behavioural change intended. The equivalence test above is what makes it safe; write it
  before touching anything.
- **Downcasting is by enum, not RTTI** — `asFacility()` tests `Location::type` and
  `static_cast`s. Cheap, but it is a checked-by-convention cast: adding a facility kind means
  adding it to `locationIsFacility()`, or the cast silently returns `nullptr`. Keeping the
  helper the single downcast site is what makes that one edit rather than many.
- **`Location::shuttle`** currently means "shuttle at this body". Once facilities are
  locations, "docked at this facility" is the more natural home, but moving it is a
  behavioural change to `canCommissionShuttle` and `BayView::getShuttle`; keep it on the
  body in Stage 2 and revisit separately.
- **Stages 2 and 3 both change the schema.** Existing quicksaves are invalidated at each;
  only `resources/initial.db` is migrated.
