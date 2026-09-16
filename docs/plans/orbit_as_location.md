# Orbit and surface as locations

> Supersedes the "Remaining" item of [facilities_as_locations.md](facilities_as_locations.md).
> Stages 1-3 of that plan are delivered; this replaces the narrower "precise craft location"
> step with the change that makes it fall out.

## Context

Everything awkward about endpoints comes from one gap: **orbit is not a place**. A craft in
orbit and a craft on the surface are both "at Mars", so the model needs a second field —
`sublocation` — to say which, and `Endpoint` needs `EndpointState` on top to say whether to
dock. `atEndpoint()` then reconciles all three against a `CraftState`.

Make orbit a location and the apparatus disappears:

```
Mars                    LOCATION_TYPE_PLANET
  Mars Orbit            LOCATION_TYPE_ORBIT        <- in orbit, no station
    Mars Orbital        LOCATION_TYPE_ORBITAL      <- docked
    Mars Orbital 2      LOCATION_TYPE_ORBITAL
  Mars Surface          LOCATION_TYPE_SURFACE      <- landed, no station
    Mars Station        LOCATION_TYPE_RESOURCE_FACILITY
```

- `Endpoint` becomes **one field**: a location.
- `atEndpoint()` becomes `location == d.location`.
- `SublocationType` and `EndpointState` are **deleted** — surface versus orbit is which
  location you are at.
- "Is this leg a transit?" is `a->body() != b->body()`, a property of the route rather than
  of the craft type. An IOS not landing becomes a capability, not a structural distinction.
- Containment is literally true, so "am I in orbit?" is "is Mars Orbit an ancestor?" — which
  answers it for a docked *and* an undocked craft.

**Scope:** endpoints and location only. `CraftState` keeps its 14 values here; the four
positional ones become redundant but harmless. Collapsing it is the next plan, written once
this has landed — [craft_state.md](craft_state.md) is built on predicates over those 14 states
and will need rewriting against a model that exists rather than a predicted one.

### Terminology

| Term | Meaning |
|---|---|
| `craft->location` | **Exactly** where the craft is: a facility, an orbit, a surface, or `space`. Never null |
| `location->body()` | Nearest celestial-body ancestor-or-self — walks up `primary` past facility, orbit and surface |
| `location->orbit()` / `surface()` | A body's orbit / surface child |
| `asFacility(loc)` | `Facility*` if that location is one, else `nullptr` |

## Identity and lifetime

Location ids stay **dense and 0-based**, with id 0 being Sol's `space` — the "nowhere in
particular" location. Nothing persists a null location; it persists id 0 instead. That removes
the null/`0` sentinel ambiguity the loader currently comments on, by making null
unrepresentable rather than by adding a second sentinel.

Density earns `locations` its O(1) lookup back:

```cpp
// game.cpp -- placed by id, not appended, so load order does not matter
Location *Game::createLocation(System *system, int id, const char *name, LocationType type)
{
    if (id < 0) { return nullptr; }
    if (static_cast<size_t>(id) >= locations.size()) { locations.resize(id + 1); }
    locations[id] = std::make_unique<Location>(system, id, name, type);
    ...
}

Location *Game::locationByID(int id)   // O(1), and correct by construction
{
    return (id >= 0 && static_cast<size_t>(id) < locations.size()) ? locations[id].get() : nullptr;
}
```

This is **not** a revert of the Stage 1 change. Stage 1 replaced `locations[id]` with a scan
because density was an undocumented accident that returned the *wrong* location when it broke.
Placing by id makes density a property the owning class maintains, tolerant of gaps (a null
slot) and independent of load order. The invariant is asserted, not assumed.

**Destruction, when it arrives, is a flag — never a removal.** Ids are not recycled. A facility
that is destroyed keeps its slot and gains a `destroyed` marker; rendering and selection skip
it, and a craft whose endpoint was that orbital still resolves the pointer, sees the flag, and
reroutes rather than chasing a dangling one. `Game` owns the factory and removal methods, as it
already owns creation. Nothing in this change destroys anything — the flag is the shape the
model should take when it does.

## Behaviour

### Where things are

| Situation | `location` today | `location` after |
|---|---|---|
| IOS in orbit at Earth | Earth | **Earth Orbit** |
| IOS docked at Earth's orbital | Earth | **Earth Orbital** |
| Shuttle on Mars surface, no station | Mars | **Mars Surface** |
| Shuttle docked at Mars station | Mars | **Mars Station** |
| IOS in transit | Sol space | Sol space |
| Shuttle route | Mars + `EP_SURFACE_DOCKED`, Mars + `EP_ORBIT_DOCKED` | **Mars Station**, **Mars Orbital** |
| IOS route | Earth + `EP_ORBIT_DOCKED`, Mars + `EP_ORBIT_DOCKED` | **Earth Orbital**, **Mars Orbital** |
| "Orbit Mars, no station" | Mars + `EP_ORBIT` | **Mars Orbit** |

Movement becomes parent/child navigation, which is why the writers are so few:

| Transition | Location change |
|---|---|
| dock | → the facility (a child of where you were) |
| undock | → `location->primary` |
| ascend | → `body()->orbit()` |
| descend | → the target facility, or `body()->surface()` |
| transit arrival | → the endpoint's location |

### What visibly changes

| Behaviour | Today | After |
|---|---|---|
| Status line | "Orbiting Mars" / "Docked at Mars orbital" | "At **Mars Orbit**" / "Docked at **Mars Orbital**" — the noun is in the name, so `statusText` loses its own |
| Which IOS is in the Space Bay | `ios->location == facility->primary` — matches any orbital at that body | `ios->location == facility` — exact |
| Autopilot: local or transit? | `dest.location == craft->location` | `dest.location->body() != craft->body()` ⇒ transit |
| Repair / capture | `resourceFacilityAt(craft->location)` | `asFacility(craft->location)` |
| Sidebar while docked | from `craft->location` | from `craft->body()` |
| Picking a destination | picks a body, forces `EP_ORBIT` | picks a body, targets its orbital or its orbit location |
| A second orbital at one body | indistinguishable | distinct locations, distinct endpoints |

## Data

Orbit and surface are persisted as `bodies` rows — they are locations, stored like locations,
ids explicit and stable rather than generation-order-dependent.

New `LocationType` values, **appended** (0-7 are persisted):

```cpp
LOCATION_TYPE_ORBIT,    // 8 -- the space around a body; parent of its orbitals
LOCATION_TYPE_SURFACE,  // 9 -- the ground; parent of its surface facilities
```

Migration of `resources/initial.db`, ids continuing the existing sequence from 186 so no
facility id moves (bodies 0-172, facilities 173-185, new rows 186+, all dense):

1. For every star, planet, moon and asteroid belt — an **orbit** row (`primary_id` = the body,
   small `orbital_radius`, `radius` 0 so it is invisible).
2. For every planet, moon and asteroid belt — a **surface** row (`orbital_radius` 0). Stars get
   none.
3. Repoint each facility: `facilities.location_id` = the orbit or surface child of its current
   body, chosen by its existing `sublocation`.
4. Drop `facilities.sublocation` — now derivable from the parent's type.
5. `craft_destinations` drops `state`, keeping only the location.

Roughly 163 orbit + 154 surface rows, taking the count from 173 to ~490.

**Ids stay dense, but stop being grouped by system.** Today each system's bodies happen to be a
contiguous id range (system 1 is 0-37, system 2 is 38-42, and so on), broken only by the nine
`space` bodies appended at 164-172. Appending orbit and surface rows scatters each system across
three ranges — its bodies, its new rows, its facilities. Density is what the O(1) lookup needs
and that is preserved; per-system *contiguity* is a separate property, wanted only by a render
sweep, and it is deferred rather than paid for speculatively. Renumbering into per-system blocks
stays cheap for as long as saved games are discardable — the standing assumption throughout this
work — so the window does not close here.

## Design

```cpp
// location.h
inline bool locationIsBody(LocationType t);   // STAR | PLANET | MOON | ASTEROID_BELT | SPACE

Location *body() const;      // walk up primary while !locationIsBody(type), depth-capped
Location *orbit() const;     // the LOCATION_TYPE_ORBIT child, or nullptr
Location *surface() const;   // the LOCATION_TYPE_SURFACE child, or nullptr
inline bool inOrbit() const; // this or an ancestor is LOCATION_TYPE_ORBIT
```

```cpp
// waypoint.h -- EndpointState and SublocationType deleted
class Endpoint
{
public:
    Location *location{nullptr};
    Endpoint() = default;
    explicit Endpoint(Location *loc) : location{loc} {}
    const char *description(char *dest, size_t len) const;
};
```

```cpp
// craft.h
inline bool atEndpoint() const
{
    const Endpoint &d = currentDestination();
    return d.location != nullptr && d.location == location;
}
```

`Facility::sublocation` is replaced by `primary->type`; `Game::facilityAt(Endpoint)` is deleted
— `asFacility(endpoint.location)` is the whole of it. `Autopilot::update` decides by route
rather than craft type:

```cpp
if (dest.location->body() != craft->body()) { craft->engageDrive(); }  // interplanetary
else { /* local: ascend, descend, dock -- all within one body */ }
```

**Shuttle ownership**, carried over and still required: `createShuttle(loc)` uses one argument
for both position and owner, so a persisted docked shuttle would reload owned by a facility
while every reader looks at the body. `Game` takes ownership as it already does for IOS
(`Shuttles` = `vector<ShuttlePtr>`), `Location::shuttle` becomes a non-owning pointer on the
**body**, and `createShuttle` takes position and derives the owner from `body()`.

## Files

- [include/state/location.h](../../include/state/location.h) / [location.cpp](../../src/state/location.cpp) — new types, `body()`, `orbit()`, `surface()`, `inOrbit()`
- [include/state/waypoint.h](../../include/state/waypoint.h) / [waypoint.cpp](../../src/state/waypoint.cpp) — `Endpoint` to one field; delete `SublocationType`, `EndpointState`
- [include/state/facility.h](../../include/state/facility.h) — delete `sublocation`; add `asFacility()`
- [include/state/craft.h](../../include/state/craft.h) / [craft.cpp](../../src/state/craft.cpp) — `atEndpoint`, the five location writers, `setDestination`, `engageAutopilot`, `statusText`
- [src/state/autopilot.cpp](../../src/state/autopilot.cpp) — route-based local-vs-transit
- [include/state/game.h](../../include/state/game.h) / [game.cpp](../../src/state/game.cpp) — `createLocation` places by id; `locationByID` O(1); delete `facilityAt`; facility factories parent onto orbit/surface; repair and capture via `asFacility`; shuttle ownership
- [src/pages/](../../src/pages/) — `overlay.cpp`, `view_state.cpp`, `bay_view.cpp`, `shuttle_view.cpp`, `drone_control_view.cpp`
- [src/loaders/](../../src/loaders/) — body types, facility parenting, `craft_destinations`, non-null location
- `resources/initial.db` — the migration above
- [tests/](../../tests/) — the `atEndpoint` matrix and craft round-trip encode today's meaning

## Migration order

**All eight steps are delivered.** Build and test after each step: breakages here are silent —
`orbitalAt`/`resourceFacilityAt` return `nullptr` when handed the wrong location kind — so each
fix lands while still a no-op.

1. **New types and helpers.** `LOCATION_TYPE_ORBIT/SURFACE`, `body()`, `orbit()`, `surface()`,
   `asFacility()`. Additive.
2. **`createLocation` places by id; `locationByID` back to O(1).** Assert density. No behaviour
   change while ids already are dense.
3. **Shuttle ownership to `Game`.** Independent, and what makes step 6 safe.
4. **Data migration + facility parenting.** Orbit/surface rows exist, facilities hang off them,
   `Facility::sublocation` becomes `primary->type` — values callers see are unchanged.
5. **Readers to `body()` / `asFacility()`** — overlay, view state, repair, capture, bay, status
   text, drone view. Each a no-op while craft location is still a body.
6. **Flip the craft location writers.** Behavioural: dock, undock, ascend, descend, arrive.
7. **Endpoints to bare locations.** Delete `EndpointState`, then `SublocationType`, then
   `facilityAt`. `atEndpoint` becomes the one-line compare.
8. **Persistence**: drop `craft_destinations.state` and `facilities.sublocation`; require a
   non-null location, persisting id 0 for "nowhere". `Facility::sublocation()` survives as a
   derived accessor — it configures the stores, factory and bay pages — but is no longer a
   stored field. `createShuttle` and `createIOS` reject a null location.

## Verification

`make && make tests && ./bin/Debug/tests`. The existing 40 cases are the guard; several encode
today's meaning and must be updated deliberately in steps 6-7.

1. **Hierarchy.** Every planet and moon has exactly one orbit and one surface child; every
   facility's parent is one of those; `body()` from any facility returns the planet.
2. **Density.** `locations[i]->id == i` for every slot, and `locationByID` agrees with a linear
   search over the whole set — the invariant that earns the O(1) lookup.
3. **Positions unchanged.** `tests/expected_positions.txt` still matches — orbit and surface
   locations must not perturb existing bodies.
4. **The rule holds.** A shuttle driven dock → work → undock → ascend → descend reports the
   expected location at each step.
5. **Autopilot completes a cycle** — the silent-failure regression. A shuttle runs a full
   station↔orbital loop and `destination_index` advances past the first dock.
6. **Both route shapes.** A shuttle between two facilities at one body never engages its drive;
   an IOS between orbitals at two bodies does.
7. **Two orbitals at one body** are distinct endpoints, and the Space Bay picks the right one.
8. **Round trip.** A docked shuttle reloads docked, owned by `Game`, referenced from its body;
   endpoints naming facilities survive; the body count is stable across three cycles.

**Play-test** — the outstanding gap across Stages 1-3, now overdue: Sol renders with moons
placed correctly and no new visible bodies, the picker still selects bodies, the sidebar stays
populated while docked, the Space Bay shows a docked IOS, F5/F8 round-trips.

## Risks

- **Silent failure is the mode.** Nothing crashes when a lookup gets the wrong location kind; it
  returns null and a feature quietly stops. The step ordering exists for that reason.
- **The data migration is the risky artefact**, not the code. ~317 inserted rows and every
  facility repointed. Verify by hierarchy assertion (test 1), not by eye, and keep the
  pre-migration `initial.db` recoverable through git.
- **`craft_state.md` is now partly obsolete.** Its capability/fitment/situation tiers survive,
  but its predicates are written against 14 states about to become far fewer. Mark it, and
  rewrite it as the next plan rather than implementing it as it stands.

## Deferred

**Page titles, and the header breadcrumb behind them.** `SublocationType` is gone — pages are
configured with `LOCATION_TYPE_ORBIT` / `LOCATION_TYPE_SURFACE`, and `Facility::sublocation()`
is replaced by `Location::inOrbit()`. Its name array went with it, so three titles lost their
orbit/surface prefix: "Orbital Stores" → "Stores", "Surface Factory" → "Factory", "Orbital
Shuttle Bay" → "Shuttle Bay". TODOs mark all three.

The prefix is redundant rather than missing. The header is already a breadcrumb — system,
location, title ([overlay.cpp:86-91](src/pages/overlay.cpp#L86-L91)) — and facilities are named
`"<body> <label>"` with labels "Orbital", "Station", "City", against regions named "Earth Orbit"
and "Earth Surface". So the side is already in the location name and the complete title comes
together across the three fields:

```
Sol   Earth Orbital   Shuttle Bay
Sol   Earth Station   Shuttle Bay
Sol   Earth Orbit     -- in orbit, no station
```

What blocks it is that the header draws `viewState.getCurrentLocation()`, deliberately pinned
to the **body** so the standard buttons keep finding the body's facilities and shuttle
([overlay.cpp:71-74](src/pages/overlay.cpp#L71-L74),
[view_state.cpp:10-12](src/pages/view_state.cpp#L10-L12)). The fix is the focus rework
`ViewState`'s own comment already anticipates: carry the precise location for display, keep the
body for the sidebar. The two are one `body()` call apart.

While there: [view_state.cpp:34-39](src/pages/view_state.cpp#L34-L39) `setCraftFocus` finds the
facility by trying `orbitalAt` then `resourceFacilityAt`. `orbitalAt` matches on `body()`, so a
craft docked at Earth Station reports Earth Orbital. Since step 6 the craft's location IS the
facility when docked, so this is `asFacility(c->location)` and the fallback goes.

**Struct-of-array positioning — a phase after the craft state rework.** The location count
roughly triples, and `System::update` plus the orrery's draw and hit-test loops walk every one
each frame. ~500-600 struct traversals is not terrible, so this is a deliberate follow-on, not
a blocker.

The shape: move the position properties (`position`, `orbital_radius`, `orbital_velocity`,
`initial_angle`) onto a parallel array owned by `Game`, indexed by location id, so the per-frame
update is a cache-friendly sweep rather than a pointer chase through `Location` objects.

Per-system id contiguity belongs to this phase, not to the migration above. Only one system
renders at a time, so the sweep wants that system's entries adjacent; renumbering into
per-system blocks would let `System` hold a `{first_id, count}` range and slice the array
directly. Doing it here rather than earlier means it is driven by a measurement instead of a
guess, and it stays cheap while saved games are discardable.

**On the Stage 1 arrays.** `System` held those arrays for a real reason: only one system is
rendered at a time, so its bodies being contiguous made the render sweep cache-friendly. That
rationale still holds. What Stage 1 removed was not the layout but the *ownership* — the arrays
were indexed by a hand-maintained, unasserted `Location::index` with a fixed 64-body cap, so the
data could only be reached through an invariant nothing checked.

Ownership and locality are separable. `Game` can own a position array indexed by location id and
still give the renderer a contiguous per-system slice, with `System` holding a range rather than
the data. That is safe in the way `Location::index` was not, because the density invariant above
is maintained and asserted rather than assumed — see the id-layout note under Data. See also the
orrery position-caching entry in [todo.md](../../todo.md).
