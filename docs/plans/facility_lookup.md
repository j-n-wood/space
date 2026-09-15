# Per-location facility lookup

> **Status: subsumed — do not implement separately.**
> [facilities_as_locations.md](facilities_as_locations.md) makes facilities child `Location`s,
> so the per-location collection proposed here becomes `Location::children` and the scan is
> over the same 1–4 entries. The conclusion holds; the hierarchy supplies the mechanism.
>
> A *different* facility lookup — "which facilities are **near** some location", for faction
> AI target selection — is not covered by either plan and does not exist yet. That change
> enables it (facilities gain resolved positions), so re-assess when the need is real.
>
> The prerequisites recorded at the end of this document remain accurate and are worth
> keeping: facility identity, and the derived-`sublocation` persistence hazard.

## Context

`Game::orbitalAt()` and `Game::resourceFacilityAt()`
([game.cpp:86](../../src/state/game.cpp#L86), [:111](../../src/state/game.cpp#L111)) scan the
whole `bases` / `orbitals` vector to answer a question about one location — both carry a
`// Initial impl is scan (yuck)` comment. There are 22 + 17 call sites, roughly 10–30
executed per frame.

This is not yet a performance problem: 13 facilities today against 173 locations, so the
scans are trivial. It becomes one as rival factions add facilities, and
[future.md](../../future.md) ("Planets are big") plans multiple resource stations, shuttles
and orbitals per body under some arbitrary limit.

**Intended outcome:** the location answers the question itself. A collection rather than a
single pointer, so the shape does not need redoing when multiplicity lands — at which point
a scan of 1–4 local entries is still cheaper than map or hash overhead.

**Scope: lookup only.** No schema change, no signature change, no behaviour change beyond
one bug fix noted below. Two prerequisites for the *later* multiplicity work were found
during investigation and are recorded at the end, not addressed here.

> **Sequencing.** [facility_type.md](facility_type.md) removes `SLOC_EARTH_CITY`, putting
> Earth City at `SLOC_SURFACE` like any other surface facility. Doing that first deletes the
> surface pairing rule below — the filter becomes a plain `f->sublocation == s` equality and
> `sublocationIsSurface()` is not needed at all. The rest of this plan is unchanged either
> way; only the filter body and its regression test differ.

## Design

### `Location` gains a non-owning collection

`Game` keeps ownership via `bases` and `orbitals`; the location holds references. This
mirrors the existing `Location::shuttle`
([location.h:64](../../include/state/location.h#L64)), which is why `locationHasShuttle()` is
already O(1).

```cpp
// include/state/location.h -- forward declaration only; facility.h already
// forward-declares Location, so there is no cycle.
class Facility;

class Location
{
    ...
    ShuttlePtr shuttle;                  // existing, owning

    // Non-owning references to the facilities at this body. Game owns them via
    // `bases` and `orbitals`. Small by design -- future.md caps facilities per
    // body at an arbitrary limit, so a scan here beats any keyed container.
    std::vector<Facility *> facilities;

    Facility *facilityAtSublocation(SublocationType s) const;
};
```

```cpp
// src/state/location.cpp -- add #include "state/facility.h"; the header only
// forward-declares Facility, but the body dereferences it.
Facility *Location::facilityAtSublocation(SublocationType s) const
{
    for (Facility *f : facilities)
    {
        if (sublocationIsSurface(s) ? sublocationIsSurface(f->sublocation)
                                    : f->sublocation == s)
        {
            return f;
        }
    }
    return nullptr;
}
```

### The surface pairing rule (only if `facility_type.md` has not landed)

If [facility_type.md](facility_type.md) is done first, skip this section — the filter is
`f->sublocation == s` and there is nothing to pair.

`EarthCity` sets `sublocation = SLOC_EARTH_CITY`
([earth_city.cpp:7](../../src/state/earth_city.cpp#L7)) but lives in `bases` and is found by
`resourceFacilityAt()` today, because that function matches on `location` alone and ignores
sublocation. Filtering on sublocation without this rule silently drops Earth City.

```cpp
// include/state/waypoint.h, beside SublocationType
// Earth City is a surface facility carrying its own sublocation tag. Anything
// asking for "the surface facility" means either.
inline bool sublocationIsSurface(SublocationType s)
{
    return s == SLOC_SURFACE || s == SLOC_EARTH_CITY;
}
```

In-memory `sublocation` is always constructor-set and correct
(`ResourceFacility`→`SLOC_SURFACE`, `EarthCity`→`SLOC_EARTH_CITY`, `Orbital`→`SLOC_ORBIT`),
including on load, since the loader goes through the same factories. Only the *persisted*
value is derived (see Prerequisites), which this change does not rely on.

### `Game`'s three accessors delegate

Signatures unchanged, so no call site moves. The `static_cast` is safe because the factory
that inserts each pointer knows its type and the sublocation is constructor-set.

```cpp
ResourceFacility *Game::resourceFacilityAt(Location *location) const
{
    if (!location) { return nullptr; }
    return static_cast<ResourceFacility *>(location->facilityAtSublocation(SLOC_SURFACE));
}

Orbital *Game::orbitalAt(Location *location) const
{
    if (!location) { return nullptr; }
    return static_cast<Orbital *>(location->facilityAtSublocation(SLOC_ORBIT));
}

Facility *Game::facilityAt(const Endpoint &endpoint)
{
    if (!endpoint.location) { return nullptr; }
    return endpoint.location->facilityAtSublocation(endpoint.sublocation);
}
```

`resourceFacilityAt` becomes `const` to match `orbitalAt`, which already is.

`facilityAt` currently sends any non-`SLOC_SURFACE` endpoint to `orbitalAt`, so an
`SLOC_EARTH_CITY` endpoint resolves to an orbital. Delegating fixes that. Nothing constructs
such an endpoint today, so the risk is nil — the only route is the
`SublocationType(1 - facility->sublocation)` arithmetic at
[game.cpp:289](../../src/state/game.cpp#L289), which yields `-1` for an Earth City and is
listed as a prerequisite below.

### Maintained in the three factory methods

All three already hold the `Location *`. Each appends, and refuses a duplicate — which makes
the existing "one orbital / one RF per location" rule an enforced statement rather than a
convention buried in `canActivatePod`. This mirrors `createShuttle`, which already does
exactly this ([game.cpp:256-260](../../src/state/game.cpp#L256-L260)):

```cpp
Orbital *Game::createOrbital(Location *location)
{
    if (!location) { return nullptr; }
    if (location->facilityAtSublocation(SLOC_ORBIT))
    {
        TraceLog(LOG_ERROR, "Blocked createOrbital: %s already has one", location->name);
        return nullptr;
    }
    orbitals.emplace_back(std::make_unique<Orbital>(location));
    auto o = orbitals.back().get();
    location->facilities.push_back(o);
    createFactory(o);
    return o;
}
```

Same shape in `createResourceFacility` and `createEarthCity`, both guarding on
`SLOC_SURFACE` (which covers Earth City via the pairing rule — a body gets one or the other,
not both).

## Files

| File | Change |
|---|---|
| [include/state/waypoint.h](../../include/state/waypoint.h) | add `sublocationIsSurface()` |
| [include/state/location.h](../../include/state/location.h) | forward-declare `Facility`; add `facilities` vector and `facilityAtSublocation()` |
| [src/state/location.cpp](../../src/state/location.cpp) | include `facility.h`; implement `facilityAtSublocation()` |
| [include/state/game.h](../../include/state/game.h) | `resourceFacilityAt` becomes `const` |
| [src/state/game.cpp](../../src/state/game.cpp) | rewrite the three accessors as delegations; append + duplicate guard in `createOrbital`, `createResourceFacility`, `createEarthCity` |
| [tests/test_db.cpp](../../tests/test_db.cpp) or new `tests/test_facility_lookup.cpp` | see below |

**Unchanged:** ownership (`bases`, `orbitals`), the SQLite schema, all 39 call sites,
`Location::shuttle`, and every accessor signature except the added `const`.

A new test *file* needs `./reconf.sh` (premake globs `tests/**.cpp` at configure time);
adding cases to `test_db.cpp` does not.

## Verification

**Existing regression suite.** `test_db.cpp` already exercises these accessors after a load
round-trip at lines 205-208, 345-358, 417-468, 599-672, 738-771, 870 and 1027. They must pass
untouched — that is the main safety net, since the loader builds the collections through the
factories.

**New cases:**

1. After `createResourceFacility(loc)` and `createOrbital(loc)`, `loc->facilities.size() == 2`;
   `orbitalAt(loc)` and `resourceFacilityAt(loc)` each return the right one.
2. `createEarthCity(loc)` is found by `resourceFacilityAt(loc)` — the pairing rule.
   Regression for the trap this design steps around.
3. A second `createOrbital` at the same location returns `nullptr`, logs, and leaves
   `facilities.size()` unchanged; likewise a second surface facility, and an Earth City on a
   body that already has a resource facility.
4. `facilityAt(Endpoint{loc, SLOC_ORBIT, true})` and `{loc, SLOC_SURFACE, true}` resolve
   correctly; `{loc, SLOC_EARTH_CITY, true}` now returns the Earth City rather than an
   orbital.
5. `orbitalAt(nullptr)` / `resourceFacilityAt(nullptr)` return `nullptr`.
6. Save/load round-trip: after `SaveGame` then a fresh `Loader`, every location's `facilities`
   is repopulated and the accessors agree with the pre-save game.

**Build and play:** `make && make tests && ./bin/Debug/tests`, then
`./bin/Debug/space` — visit Earth (surface and orbital pages both reachable), Mars, and
Jupiter; confirm the standard buttons still appear, since
[base_page.cpp:85-86](../../src/pages/base_page.cpp#L85-L86) drives them off both accessors
every frame. F5/F8 quicksave and quickload.

## Prerequisites for the multiplicity work (not in this change)

Found while investigating; both block naming a *specific* facility in an `Endpoint`, which is
what the planned shuttle transfer-between-facilities states will need.

1. **Facilities have no stable identity.** `Facility::id` is initialised to `0`, never
   assigned at runtime by any factory, and renumbered `1..N` at every save by
   `nextFacilityId++` ([save_game.cpp:218](../../src/loaders/save_game.cpp#L218)) — bases then
   orbitals — with no write-back to the in-memory object. The loader does restore it
   (`fac->id = id`, [loader.cpp:167](../../src/loaders/loader.cpp#L167)), so a save/load
   round-trip is self-consistent; what is missing is an identity for a facility created
   during play, which is `0` until the next save renumbers everything. Craft do this properly
   with `id = ++craft_max_id` ([game.cpp:264](../../src/state/game.cpp#L264)); facilities
   need the equivalent before an endpoint can name one. Note this also bears on the
   suggestion in
   [waypoint.h:19](../../include/state/waypoint.h#L19) that "an index can be provided" — an
   index into a collection with holes breaks once mass drivers make facilities destroyable, so
   a real allocated id is the better key.
2. **`sublocation` is persisted as a derived value** — now addressed by
   [facility_type.md](facility_type.md), which stores type and sublocation separately. For
   the record: `saveBase` writes
   `training_facility || research_facility ? SLOC_EARTH_CITY : SLOC_SURFACE`
   ([save_game.cpp:470-474](../../src/loaders/save_game.cpp#L470-L474), marked "somewhat hacky
   //TODO"). Harmless for this change, which only reads the constructor-set in-memory field,
   but it must be fixed before sublocation carries more meaning. Related: the
   `SublocationType(1 - facility->sublocation)` arithmetic at
   [game.cpp:289](../../src/state/game.cpp#L289) yields `-1` for an Earth City.

Also relevant when multiplicity lands: the shuttle transfer states are the first real
customer for the `-Wswitch` property in [craft_state.md](craft_state.md) §2 — adding
`CS_SURFACE_TRANSFER` / `CS_ORBIT_TRANSFER` will make the compiler flag every predicate that
has not considered them. `CA_TRANSFER` likely needs to be *two* actions (surface / orbit) so
that `checkCapability`'s state-independent table stays honest, and `perform()` will need a
target facility parameter it does not currently take.
