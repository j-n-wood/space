# Craft state model refactor

> Follows [orbit_as_location.md](orbit_as_location.md), which has landed. That change made
> `craft->location` name an exact place, which takes two of `CraftState`'s three dimensions
> out of the enum entirely — see [§2](#2-what-orbit-as-location-does-to-the-enum). The
> capability / fitment / situation tiers and the single guarded mutator are unchanged from
> the original plan; the state list, the predicates and the expiry switch are re-derived
> against the current model.

## Context

`CraftState` (14 values) is currently driven from four places that each re-implement the
same rules differently: the expiry `switch` in [shuttle.cpp:14-68](../../src/state/shuttle.cpp#L14-L68),
a divergent subset of it in [ios.cpp:9-42](../../src/state/ios.cpp#L9-L42), its own copy of
the dock/descend/transit rules in [autopilot.cpp:111-154](../../src/state/autopilot.cpp#L111-L154),
and two further copies in `ShuttleView` — mouse in `render()`, keyboard in `input()` — that
disagree with each other. Callers write `state = X; state_timer = Y;` directly, leaving
`total_state_timer` stale.

**Goal:** one timed-transition mechanism, and one permission check asked the same way by the
player, the autopilot and the game logic — over a state enum that holds each fact once.

### The model

Three questions are conflated today into scattered `type == CT_SHUTTLE`, `if (drive)` and
`state == CS_X` tests. They are separated here:

| Tier | Question | Varies with | Example refusal |
|---|---|---|---|
| **Capability** | Is this hull *designed* to do it? | `CraftType`, permanent | a shuttle can never make an interplanetary transit |
| **Fitment** | Is the equipment *installed*? | per craft, changes in the bay | no drive fitted → cannot dock, launch, ascend, descend |
| **Situation** | Is it allowed *right now*? | state + world | not in orbit; orbital incomplete; defended by drones |

`drive` is the engine, so every manoeuvre needs it — not just transit. `CC_INTERPLANETARY` is
separately whether the hull is *rated* to cross interplanetary space. A shuttle therefore
legitimately carries a drive (it needs engines to ascend and descend) while never being able
to transit.

**Not in scope:** no UI is built to display a refusal reason. A control whose action is
refused is simply not drawn, as `ShuttleView` already does. The reason is still carried
through the guards — it drives logging, and leaves the door open. See [Deferred](#deferred).

---

## 1. `craft_action.h` / `.cpp` — **implemented**

See [include/state/craft_action.h](../../include/state/craft_action.h) and
[src/state/craft_action.cpp](../../src/state/craft_action.cpp). Free of `game.h`, so it is
unit-testable without a `Game`.

`CraftType` lives in its own [craft_type.h](../../include/state/craft_type.h), included by
both `craft.h` and `craft_action.h`. That is what keeps the two independent: `craft_action.h`
needs only `CraftType` and `CT_COUNT`, so it does not include `craft.h`, and step 2 can add
`perform()` plus the inline verb wrappers to `Craft` — which need the complete
`CraftActionResult` — without a cycle. Verified by compiling both include orders.

Three deliberate departures from the original sketch:

- **`CA_UNDOCK` became `CA_LAUNCH`** — "leave the current location", which covers undocking from
  an orbital and lifting off a surface station with one action. `CA_DISENGAGE_DRIVE` was
  added to pair with `CA_ENGAGE_DRIVE`.
- **Capabilities describe the flight envelope, not the manoeuvre list**:
  `CC_ATMOSPHERIC` / `CC_INTERPLANETARY` / `CC_INTERSTELLAR` / `CC_BOARDING`. There is no
  `CC_DOCK_ORBIT`: every hull can dock, so docking is capability-free and gated only by the
  situation tier. `CC_INTERSTELLAR` is forward-looking, held by `CT_SCG` alone.

| | `CT_SHUTTLE` | `CT_IOS` | `CT_SCG` |
|---|---|---|---|
| `CC_ATMOSPHERIC` | yes | — | — |
| `CC_INTERPLANETARY` | — | yes | yes |
| `CC_INTERSTELLAR` | — | — | yes |
| `CC_BOARDING` | — | yes | yes |

So a shuttle cannot engage its drive (no `CC_INTERPLANETARY`) and an IOS can neither descend
nor ascend (no `CC_ATMOSPHERIC`) — both refusals now come from one table.

**Open: array sizing.** `craftActionNames`, `craftCapabilities` and `craftActionCodeText`
are declared `[CA_COUNT]` / `[CT_COUNT]` / `[CAC_COUNT]` and initialised positionally, so a
short initialiser list zero-fills the tail rather than failing — adding an enum value without
its string gives `text()` a `nullptr`. Sizing from the initialiser and asserting the length
turns that into a compile error:

```cpp
const char *craftActionNames[] = { ... };
static_assert(sizeof craftActionNames / sizeof *craftActionNames == CA_COUNT, "...");
```

All three are complete today (9 / 3 / 12), so this guards the next edit rather than fixing a
break.

**`actionNeedsDrive` is deliberately not implemented yet**, to be judged during
implementation since nearly every action needs a drive. Consequence to keep in view:
`CAC_NO_DRIVE` is unreachable until it exists, so the fitment tier is currently a no-op. If
it does turn out to be "everything except `CA_NONE`, `CA_WORK`, `CA_CANCEL_WORK`", it wants
to be a predicate rather than a parallel table — a table indexed by action is a second thing
to keep in step with the enum.

**Newly enforced.** Three rules previously held only by being awkward to violate:

- An IOS cannot descend, therefore cannot ascend — both need `CC_ATMOSPHERIC`. Ascent was never
  checked anywhere before.
- A shuttle cannot ascend or descend without a drive.
- Docking and launching need a drive.

`CA_LAUNCH` needing a drive means a craft whose drive is removed or damaged is stranded at
its station. That is the intended reading, and the hook for damaged drives later.

---

## 2. What orbit-as-location does to the enum

`CraftState`'s 14 values are a product of **position × docked × activity**. Two of those
three are now properties of `craft->location`, which since
[orbit_as_location.md](orbit_as_location.md) names an exact place rather than a body:

| Question | Was | Now |
|---|---|---|
| Which side of the body? | `CS_SURFACE*` vs `CS_ORBIT*` | `location->inOrbit()` |
| Docked? | the `*_DOCKED` / `*_DOCK_WORK` arms | `location->isFacility()` |
| Doing what? | folded into the same 14 values | **all `CraftState` is left carrying** |

So the enum collapses to seven — activity alone:

```cpp
enum CraftState : uint8_t
{
    CS_IDLE,       // at rest wherever location says: region or facility, orbit or surface
    CS_WORKING,    // pods tick; docked = at a station, undocked = building one
    CS_LAUNCHING,  // leaving a facility
    CS_ASCENDING,  // surface region -> orbit region
    CS_DESCENDING, // orbit region -> surface region
    CS_DOCKING,    // approaching a facility
    CS_TRANSIT,    // between bodies; location is the system's `space`
    CS_COUNT
};
```

| Today | Becomes | Position now read from |
|---|---|---|
| `CS_SURFACE`, `CS_ORBIT` | `CS_IDLE` | the region it is in |
| `CS_SURFACE_DOCKED`, `CS_ORBIT_DOCKED` | `CS_IDLE` | the facility it is in |
| `CS_SURFACE_WORK`, `CS_ORBIT_WORK` | `CS_WORKING` | the region — **building** |
| `CS_SURFACE_DOCK_WORK`, `CS_ORBIT_DOCK_WORK` | `CS_WORKING` | the facility — working at a station |
| `CS_SURFACE_LAUNCH`, `CS_ORBIT_LAUNCH` | `CS_LAUNCHING` | the region, after `launch()` steps out |
| `CS_ASCENDING`, `CS_DESCENDING`, `CS_ORBIT_DOCKING`, `CS_TRANSIT` | unchanged in meaning | — |

**This removes a class of bug rather than just shortening a list.** Under 14 values, state
and location were two independent records of the same fact and could disagree — `state ==
CS_ORBIT_DOCKED` while `location` was a bare region was representable, and step 6 of the
orbit work produced exactly that twice. Deriving both from `location` makes the
disagreement unrepresentable.

### Predicates (`craft.h` / `craft.cpp`)

Two of the four stop being switches over the enum:

```cpp
// Where, straight from the hierarchy. No enum arm to forget, and they cannot
// contradict the craft's actual position because they ARE its actual position.
bool Craft::docked()  const { return location && location->isFacility(); }
bool Craft::inOrbit() const { return location && location->inOrbit(); }

bool Craft::working() const { return state_ == CS_WORKING; }

bool Craft::moving() const
{
    switch (state_)
    {
    case CS_LAUNCHING:
    case CS_ASCENDING:
    case CS_DESCENDING:
    case CS_DOCKING:
    case CS_TRANSIT:
        return true;
    case CS_IDLE:
    case CS_WORKING:
    case CS_COUNT:
        return false;
    }
    return false;
}
```

`moving()` stays an exhaustive `switch` **with no `default:`** — clang emits `-Wswitch` by
default on this toolchain (verified; premake sets no extra warning level), so an eighth
`CraftState` warns here rather than silently reading false. A lookup table would give a
silent zero for the missing row.

`atEndpoint()` is already a location compare in the code
([craft.cpp](../../src/state/craft.cpp#L112)) and needs nothing from this plan:

```cpp
    // Both sides name a precise location, so orbit, surface and docked are all
    // implied by which one it is.
    if (dest.location == location) { return true; }
    // One tolerance: sent to a region, ended up docked at a station inside it.
    return location->isFacility() && location->primary == dest.location;
```

`moving()` means the drive is running, which is *why* commands are refused. `working()`
covers building as well as station work — see below, which is the requirement it exists to
meet rather than a harmless generalisation.

### Building states: what `CS_WORKING` undocked means

The four working states become one, and the distinction between them is read from where the
craft is. That is not a loss of information — it is the same fact, held once:

| `location` is | `CS_WORKING` means | Pod |
|---|---|---|
| a facility | working at an existing station | `Bandaid` (repair) |
| an orbit region | **building the orbital** | `Of_Frame` |
| a surface region | **building the resource facility** | `R_Frame` |

**The old `CS_SURFACE_WORK` / `CS_ORBIT_WORK` were unimplemented, not dead.** Nothing in
`src/` assigns them today, but they are where facility construction and pod activation
belong, and building is precisely the case with nothing to dock at: `Of_Frame` *creates* the
orbital, so the craft is necessarily in the bare orbit region while it works. Since orbit
and surface became locations that is literal — `craft->location` is the region, and the new
facility appears as a child of it.

Two gaps to close when this is implemented:

- [`Game::activatePod`](../../src/state/game.cpp#L786) applies a whole construction increment
  instantly and sets no state at all. It should start a timed `CS_WORKING`, exactly as its
  `Bandaid` branch already starts a timed `CS_SURFACE_DOCK_WORK`.
- [`Craft::update`](../../src/state/craft.cpp#L164) ticks pods only for the two `*_DOCK_WORK`
  states, so `Game::updateActivePod` is unreachable while building. That condition becomes
  `working()`.

Expiry needs no branch per side: `CS_WORKING` returns to `CS_IDLE` wherever the craft
already is, and `onDockWorkComplete()` fires only `if (docked())`. `statusText` covers both
("Working in Earth Orbit", "Working at Earth Orbital").

---

## 3. `Craft`: private state, one guard, one mutator

```cpp
class Craft
{
    // The state machine. Nothing outside Craft may assign these.
    CraftState state_{CS_IDLE};
    float      state_timer_{0.0f};
    float      total_state_timer_{0.0f};   // both were uninitialised in the ctor

    // The ONLY writer. Its whole job is keeping the two timers in step -- that is
    // the invariant, and this is the one place it can be broken. setState(s) was
    // only setTimedState(s, 0.0f), so it is gone in favour of a default argument.
    void setTimedState(CraftState s, float duration = 0.0f)
    {
        state_ = s;
        state_timer_ = duration;
        total_state_timer_ = duration;
    }

    bool beginTransit();                    // was the body of engageDrive()
    uint32_t requiredActionsForRoute() const;

public:
    CraftState state() const      { return state_; }
    float      stateTimer() const { return state_timer_; }

    // 0..1 through the current timed manoeuvre, 0 when untimed. Replaces two
    // hand-rolled divisions, one of which could divide by zero.
    float progress() const
    {
        return total_state_timer_ > 0.0f ? 1.0f - state_timer_ / total_state_timer_ : 0.0f;
    }

    // Persistence and test setup only: restores a craft, running no guard and
    // firing no event. The single deliberate hole in the encapsulation -- named
    // so it is greppable, and so a caller has to mean it.
    void restoreState(CraftState s, float timer, float total)
    {
        state_ = s;
        state_timer_ = timer;
        total_state_timer_ = total > 0.0f ? total : timer; // repair legacy saves
    }

    // Tiers 1-2 only: capability + fitment. State- and world-independent, so it
    // answers "could this craft EVER do this?" -- what the autopilot needs to
    // decide whether a route is flyable at all.
    CraftActionResult checkCapability(CraftAction action) const;

    // All three tiers.
    CraftActionResult checkAction(CraftAction action) const;
    inline bool canPerform(CraftAction a) const { return checkAction(a) == CAC_OK; }

    // The single mutator. Returns what checkAction would have returned; mutates
    // nothing unless that is CAC_OK.
    CraftActionResult perform(CraftAction action, float duration = -1.0f);

    // Readable call sites; thin wrappers, no independent logic.
    inline CraftActionResult dock()        { return perform(CA_DOCK); }
    inline CraftActionResult launch()      { return perform(CA_LAUNCH); }
    inline CraftActionResult descend()     { return perform(CA_DESCEND); }
    inline CraftActionResult ascend()      { return perform(CA_ASCEND); }
    inline CraftActionResult work(float d) { return perform(CA_WORK, d); }

    CraftActionResult engageAutopilot();
    void disengageAutopilot();

    bool docked() const;   // location->isFacility()
    bool inOrbit() const;  // location->inOrbit()
    bool working() const;
    bool moving() const;

    void update(float delta);
    virtual void onStateTimerExpired();     // virtual for the future CT_SCG
    virtual void onDocked();
    virtual void onDockWorkComplete();
};
```

The two guards are one layered on the other, so the tiers cannot drift and the reason
returned is always the most specific:

```cpp
CraftActionResult Craft::checkCapability(CraftAction action) const
{
    // CA_ASCEND needs CC_ATMOSPHERIC just as CA_DESCEND does: a craft that cannot come
    // down has no business going up. CA_LAUNCH is CC_NONE -- it serves both the
    // orbital and the surface dock, and the state tier requires you to be in one.
    static const uint16_t needs[CA_COUNT] = {
        /* CA_NONE         */ CC_NONE,
        /* CA_DOCK         */ CC_NONE, // every hull docks; the situation tier gates it
        /* CA_LAUNCH       */ CC_NONE,
        /* CA_DESCEND      */ CC_ATMOSPHERIC,
        /* CA_ASCEND       */ CC_ATMOSPHERIC,
        /* CA_ENGAGE_DRIVE */ CC_INTERPLANETARY,
        /* CA_WORK         */ CC_NONE,
        /* CA_CANCEL_WORK  */ CC_NONE,
    };

    if (!craftHasCapability(type, needs[action])) { return CAC_NOT_CAPABLE; } // tier 1
    if (actionNeedsDrive[action] && !drive)       { return CAC_NO_DRIVE; }    // tier 2
    return CAC_OK;
}

CraftActionResult Craft::checkAction(CraftAction action) const
{
    CraftActionResult result = checkCapability(action);
    if (!result) { return result; }

    // tier 3. Busy while the drive is running, or while work is under way --
    // CA_CANCEL_WORK is the deliberate escape from the latter, so undocking
    // mid-load must go through it rather than silently truncating the work.
    if (moving())                              { return CAC_BUSY; }
    if (working() && action != CA_CANCEL_WORK) { return CAC_BUSY; }

    switch (action)
    {
    case CA_DOCK:
        // in orbit, not already inside something
        if (docked() || !inOrbit()) { return CAC_WRONG_STATE; }
        return Game::getCurrent()->checkCraftCanDock(this);

    case CA_LAUNCH:
        // Must be attached to something to leave it. Leaving the ground when NOT
        // docked is CA_ASCEND -- see the note below on where the two meet.
        return docked() ? CAC_OK : CAC_WRONG_STATE;

    case CA_DESCEND:
        return (inOrbit() && !docked()) ? CAC_OK : CAC_WRONG_STATE;

    case CA_ASCEND:
        return (!inOrbit() && !docked()) ? CAC_OK : CAC_WRONG_STATE;

    case CA_ENGAGE_DRIVE:
    {
        if (docked())               { return CAC_WRONG_STATE; }
        const Endpoint &d = currentDestination();
        if (!d.location)            { return CAC_NO_DESTINATION; }
        if (d.location == location) { return CAC_WRONG_STATE; }
        return CAC_OK;
    }

    case CA_WORK:
        // Docked = work at the station; undocked in a region = build one.
        // Both are CS_WORKING; only CS_TRANSIT has nowhere to work.
        return (state_ == CS_TRANSIT) ? CAC_WRONG_STATE : CAC_OK;

    case CA_CANCEL_WORK:
        return working() ? CAC_OK : CAC_WRONG_STATE;

    case CA_NONE:
    case CA_COUNT:
        return CAC_WRONG_STATE;
    }
    return CAC_WRONG_STATE;
}

CraftActionResult Craft::perform(CraftAction action, float duration)
{
    CraftActionResult result = checkAction(action);
    if (!result) { return result; }   // caller learns why, and nothing moved

    switch (action)
    {
    case CA_DOCK:
        setTimedState(CS_DOCKING, CSTD_DOCK);
        break;
    case CA_LAUNCH:
        // One state for both sides: location->primary is the region either way.
        location = location->primary;
        setTimedState(CS_LAUNCHING, CSTD_LAUNCH);
        break;
    case CA_DESCEND:
        setTimedState(CS_DESCENDING, CSTD_DESCENT);
        break;
    case CA_ASCEND:
        setTimedState(CS_LAUNCHING, CSTD_LAUNCH);
        break;
    case CA_ENGAGE_DRIVE:
        if (!beginTransit()) { return CAC_ROUTE_UNREACHABLE; }
        break;
    case CA_WORK:
        setTimedState(CS_WORKING, duration > 0.0f ? duration : 1.0f);
        break;
    case CA_CANCEL_WORK:
        // Abandoning a build leaves the craft where it is -- in the region it was
        // building in, not docked at a station that may not exist yet.
        setTimedState(CS_IDLE);
        break;
    case CA_NONE:
    case CA_COUNT:
        return CAC_WRONG_STATE;
    }
    return CAC_OK;
}

```

**Open question: where `CA_LAUNCH` and `CA_ASCEND` meet.** `CA_LAUNCH` as guarded above
requires `docked()`, so from a surface *station* it undocks into the surface region and the
expiry switch then carries it to `CS_ASCENDING` — one press, two phases, which is what the
old `CS_SURFACE_LAUNCH → CS_ASCENDING` pair already did. But a craft sitting in a bare
surface region with no station is not docked, so only `CA_ASCEND` applies to it. Two actions
therefore reach orbit depending on whether a station is present, and the UI would need both
bound to the same control. The alternative is to let `CA_LAUNCH` mean "leave whatever you
are in, including the ground" and drop `CA_ASCEND` entirely, since `CS_LAUNCHING` already
picks its successor from `inOrbit()`. Worth settling before §6 wires the control table.

```cpp
bool Craft::beginTransit()
{
    Location *destination = currentDestination().location;
    Game *game = Game::getCurrent();

    const float t = game->transitTimeCalculator->calculateTransitTime(location, destination);
    TraceLog(LOG_INFO, "Engaging drive from %s to %s, transit time %.1f seconds",
             location ? location->name : "Space", destination->name, t);

    setTimedState(CS_TRANSIT, t);
    location = location->system->space;
    return true;
}
```

`Game::craftCanDock` becomes `checkCraftCanDock` — same rule, returning the reason:

```cpp
CraftActionResult Game::checkCraftCanDock(const Craft *craft) const
{
    Orbital *o = orbitalAt(craft->location);
    if (!o)              { return CAC_NO_ORBITAL; }
    if (!o->operational) { return CAC_ORBITAL_INCOMPLETE; }

    if (o->faction_id != craft->faction_id && factions[o->faction_id].hostile)
    {
        const int drones = o->stores.items[ItemType::Star_Drone]
                         + o->stores.items[ItemType::Ios_Drone];
        if (drones > 0) { return CAC_DEFENDED; }
    }
    return CAC_OK;
}
```

---

## 4. One countdown, one expiry switch (`craft.cpp`)

`Shuttle::update` and `IOS::update` are deleted, along with their declarations in
[shuttle.h](../../include/state/shuttle.h) / [ios.h](../../include/state/ios.h).
`shuttle.cpp` keeps only its constructor; `ios.cpp` becomes an empty translation unit.

```cpp
void Craft::update(float delta)
{
    // The countdown, previously duplicated in both subclasses. Decrementing
    // state_timer_ without touching total_state_timer_ is intended: total_ is the
    // denominator progress() divides by, so it must survive the tick. Every
    // TRANSITION goes through setTimedState, which sets both.
    if (state_timer_ > 0.0f)
    {
        state_timer_ -= delta;
        if (state_timer_ <= 0.0f)
        {
            state_timer_ = 0.0f;
            onStateTimerExpired();
        }
    }

    if (autopilot->state >= AS_ON)   // was: if (drive)
    {
        autopilot->update(this, delta);
    }

    if (working())                   // was the CS_*_DOCK_WORK pair test
    {
        Game *game = Game::getCurrent();
        for (int pod_idx = 0; pod_idx < max_pods; ++pod_idx)
        {
            if (!isPodEmpty(pod_idx))
            {
                game->updateActivePod(this, pods[pod_idx], delta);
            }
        }
    }
}

void Craft::onStateTimerExpired()
{
    Game *game = Game::getCurrent();

    switch (state_)
    {
    case CS_LAUNCHING:
        // Which way out is a property of where we are, not of a separate state:
        // undocking in orbit leaves you in orbit; leaving the ground means climbing.
        if (inOrbit()) { setTimedState(CS_IDLE); }
        else           { setTimedState(CS_ASCENDING, CSTD_ASCENT); }
        break;

    case CS_ASCENDING:
        enterRegion(true);
        setTimedState(CS_IDLE);
        break;

    case CS_DESCENDING:
        // Reached the ground either way; onDocked steps into the station if there
        // is one to dock at, leaving location to say whether we are docked.
        enterRegion(false);
        setTimedState(CS_IDLE);
        if (game->resourceFacilityAt(location)) { onDocked(); }
        break;

    case CS_DOCKING:
        setTimedState(CS_IDLE);
        onDocked();
        game->onSpacecraftDocked(this);   // was IOS-only
        break;

    case CS_WORKING:
        // Back to rest wherever we are. Docked means it was station work; a region
        // means it was construction, and the new facility is now a child of it.
        setTimedState(CS_IDLE);
        if (docked()) { onDockWorkComplete(); }
        break;

    case CS_TRANSIT:                      // was IOS-only; a shuttle hung here forever
        setTimedState(CS_IDLE);
        arriveAtLocation();
        game->onSpacecraftArrival(this);
        break;

    case CS_IDLE:                         // a timer should not have been running
    case CS_COUNT:
        break;
    }
}
```

Unifying the two switches is safe because capabilities make the extra arms unreachable per
type: a shuttle cannot enter `CS_TRANSIT`, an IOS cannot enter the surface states.
`onSpacecraftDocked` now fires for every craft; `Game::onSpacecraftDocked` gates the capture
itself on `CC_BOARDING`, so shuttle behaviour is unchanged and the rule lives in one place:

```cpp
void Game::onSpacecraftDocked(Craft *craft)
{
    if (!craftHasCapability(craft->type, CC_BOARDING))    { return; }
    if (!hostilesAt(craft->location, craft->faction_id)) { return; }

    if (Orbital *orbital = orbitalAt(craft->location))
    {
        onCaptureOrbital(orbital, craft->faction_id);
    }
}
```

---

## 5. Autopilot (`autopilot.cpp`)

Stops writing `craft->state`; asks the same question the buttons ask.

```cpp
void Autopilot::update(Craft *craft, float delta)
{
    if (state < AS_ON)                    { return; }
    if (craft->state() != CS_IDLE)        { return; }
    if (craft->docked())                  { return; }   // work or undock drives the next step

    const Endpoint &dest = craft->currentDestination();
    if (!dest.location || craft->atEndpoint()) { return; }

    // The endpoint is a location, so it IS the instruction. Different body means
    // transit; same body means the side it names says ascend, descend or dock.
    if (dest.location->body() != craft->body())
    {
        CraftActionResult r = craft->perform(CA_ENGAGE_DRIVE);
        if (!r)
        {
            TraceLog(LOG_WARNING, "Autopilot: %s cannot depart - %s", craft->name, r.text());
            state = AS_OFF;
        }
    }
    else if (dest.location->inOrbit())
    {
        // A facility means dock with it; a bare orbit region means just be there.
        if (!craft->inOrbit())            { craft->ascend(); }
        else if (dest.location->isFacility() && !craft->dock())
        {
            // covers non-operational and drone-defended, not just "no station":
            // retrying forever against a defended orbital was the old failure.
            TraceLog(LOG_WARNING, "Autopilot: %s cannot dock, holding in orbit", craft->name);
            state = AS_OFF;
        }
    }
    else
    {
        craft->descend();   // gains the CC_ATMOSPHERIC guard the mouse path had as `type ==`
    }
}

void Autopilot::onDockWorkComplete(Craft *craft)
{
    craft->launch();
}
```

`Autopilot::onDocked`'s tail becomes `craft->work(1.0f)`.

Engaging reports why it cannot — this is where a missing drive is expressed rather than
silently no-opped:

```cpp
uint32_t Craft::requiredActionsForRoute() const
{
    uint32_t needed = 0;
    const Endpoint &a = destinations[0];
    const Endpoint &b = destinations[1];
    if (!a.location || !b.location) { return needed; }

    // A route is interplanetary if its ends sit at different bodies -- a property
    // of the route, not of the craft type.
    if (a.location->body() != b.location->body()) { needed |= 1u << CA_ENGAGE_DRIVE; }

    for (const Endpoint &e : destinations)
    {
        if (!e.location) { continue; }
        if (!e.location->inOrbit())
        {
            needed |= (1u << CA_DESCEND) | (1u << CA_ASCEND);
        }
        if (e.location->isFacility())
        {
            needed |= (1u << CA_DOCK) | (1u << CA_LAUNCH);
        }
    }
    return needed;
}

CraftActionResult Craft::engageAutopilot()
{
    bool has_supply_pod = false;
    for (int i = 0; i < max_pods; ++i)
    {
        if (pods[i].type == PT_SUPPLY) { has_supply_pod = true; break; }
    }
    if (!has_supply_pod) { return CAC_NO_SUPPLY_POD; }

    // Ask -- state-independently -- whether this hull, as fitted, can fly every leg
    // the route implies. A driveless craft fails on CAC_NO_DRIVE because ascend and
    // descend need engines; a shuttle with a cross-location route on CAC_NOT_CAPABLE.
    const uint32_t needed = requiredActionsForRoute();
    for (int a = CA_NONE + 1; a < CA_COUNT; ++a)
    {
        if (!(needed & (1u << a))) { continue; }
        CraftActionResult r = checkCapability(static_cast<CraftAction>(a));
        if (!r) { return r; }
    }

    // The autopilot moves cargo, so it wants to dock at both ends: upgrade any
    // endpoint naming a bare region to the station inside it, if there is one.
    // Already implemented in Craft::engageAutopilot.
    Game *game = Game::getCurrent();
    for (int i = 0; i < MAX_DESTINATIONS; ++i)
    {
        Location *t = destinations[i].location;
        if (t && !t->isFacility())
        {
            destinations[i].location = game->targetFor(t, t->inOrbit());
        }
    }
    if (docked()) { launch(); }

    autopilot->state = AS_ON;
    return CAC_OK;
}
```

Callers log the refusal (`r.text()`). `Craft::update`'s gate becomes
`autopilot->state >= AS_ON`: not a loosening, since the drive requirement moved *earlier*,
to engagement, where it is stated.

---

## 6. `ShuttleView`: one rule set for mouse and keyboard

No changes to `Overlay` or `ui_elements.h`. The existing `if (cond && renderButton(...))`
idiom stays — a refused control is not drawn, so it neither highlights nor clicks. Only the
condition changes. `craft_can_dock` ([shuttle_view.h:26](../../include/pages/shuttle_view.h#L26))
and the render-before-input coupling its comment documents are deleted.

```cpp
// file scope, beside the other layout rectangles
static const struct
{
    CraftAction action;
    Rectangle   rect;
    const char *tip;
} craftControls[] = {
    {CA_DOCK,    {561, 838, 72, 52}, "Dock"},
    {CA_LAUNCH,  {561, 838, 72, 52}, "Launch"},   // same hotspot; states are exclusive
    {CA_DESCEND, {642, 831, 76, 57}, "Descend to surface"},
    {CA_ASCEND,  {722, 832, 76, 49}, "Ascend to orbit"},
};

// ShuttleView::render()
{
    UITransparentButtonState transparentButtonState;
    for (const auto &c : craftControls)
    {
        if (craft->canPerform(c.action) && overlay.renderButton(c.rect, "", c.tip, WHITE))
        {
            craft->perform(c.action);   // re-checks internally
        }
    }
}

// ShuttleView::input()
static const struct { int key; CraftAction actions[2]; } craftKeys[] = {
    {KEY_D, {CA_LAUNCH, CA_DOCK}},      // guards are mutually exclusive
    {KEY_A, {CA_ASCEND, CA_DESCEND}},
    {KEY_E, {CA_ENGAGE_DRIVE, CA_NONE}},
};

for (const auto &b : craftKeys)
{
    if (!IsKeyPressed(b.key)) { continue; }
    for (CraftAction a : b.actions)
    {
        if (a != CA_NONE && craft->perform(a)) { break; }
    }
}

if (IsKeyPressed(KEY_X))    // was writing autopilot->state directly
{
    if (craft->autopilot->state == AS_ON)
    {
        craft->disengageAutopilot();
    }
    else
    {
        CraftActionResult r = craft->engageAutopilot();
        if (!r) { TraceLog(LOG_WARNING, "Autopilot: %s", r.text()); }
    }
}
```

This closes the ungated `KEY_E`, the IOS-can-descend hole in `KEY_A`, and `KEY_X`'s bypass
of `engageAutopilot()`. The drive controls' `(craft->type != CT_SHUTTLE) && (craft->drive)`
test at [shuttle_view.cpp:162](../../src/pages/shuttle_view.cpp#L162) and
[:327](../../src/pages/shuttle_view.cpp#L327) — duplicated today with mismatched hit
rectangles (`source.height * 2` in `input()` vs `* 4` in `render()`) — becomes one
`canPerform(CA_ENGAGE_DRIVE)` over one shared rectangle.

Also add `if (!craft) { return; }` to `render()` and `input()`: `activate()` can leave it
null while both dereference it unconditionally.

---

## Files

**New**

- `include/state/craft_action.h`, `src/state/craft_action.cpp` (§1)
- `tests/test_craft_state.cpp`

**Modified**

| File | Change |
|---|---|
| [include/state/craft.h](../../include/state/craft.h), [src/state/craft.cpp](../../src/state/craft.cpp) | §2, §3, §4; private `state_`/timers; `setState` removed; `engageDrive` → `beginTransit` |
| [include/state/shuttle.h](../../include/state/shuttle.h), [include/state/ios.h](../../include/state/ios.h) | drop the `update` overrides |
| [src/state/shuttle.cpp](../../src/state/shuttle.cpp), [src/state/ios.cpp](../../src/state/ios.cpp) | delete both `update` bodies |
| [include/state/game.h](../../include/state/game.h), [src/state/game.cpp](../../src/state/game.cpp) | `craftCanDock` → `checkCraftCanDock`; `onSpacecraftDocked` gates on `CC_BOARDING`; `activatePod` / `updateActivePod` use `perform(CA_WORK)` / `perform(CA_CANCEL_WORK)` |
| [src/state/autopilot.cpp](../../src/state/autopilot.cpp) | §5 |
| [include/pages/shuttle_view.h](../../include/pages/shuttle_view.h), [src/pages/shuttle_view.cpp](../../src/pages/shuttle_view.cpp) | §6; delete `craft_can_dock`; `progress()` at :283 |
| [src/pages/bay_view.cpp](../../src/pages/bay_view.cpp) | docked tests → `craft->location == facility`, exact rather than by body |
| [src/orrery.cpp](../../src/orrery.cpp) | :155 → `craft->progress()` (absorbs the divide-by-zero guard) |
| [src/loaders/loader.cpp](../../src/loaders/loader.cpp) | direct writes → one `restoreState(...)` call |
| [src/loaders/save_game.cpp](../../src/loaders/save_game.cpp) | reads → `state()` / `stateTimer()` |
| [src/main.cpp](../../src/main.cpp), [src/pages/master_control.cpp](../../src/pages/master_control.cpp), [src/pages/drone_control_view.cpp](../../src/pages/drone_control_view.cpp), [tests/test_db.cpp](../../tests/test_db.cpp) | mechanical `->state` → `->state()`; scaffold writes → `restoreState` |
| [architecture.md](../../architecture.md) | :90-105 lists a stale 12-value enum |

**`CraftState` is renumbered**, 14 values to 7. The SQLite column is unchanged in *shape*,
but its values change meaning, so saves do not survive. That is free today:
`resources/initial.db` has **no `craft` rows** (verified), every scaffold craft is built by
`main.cpp`, and saved games remain discardable — the standing assumption throughout this
work. Migrating later would not be.

Add `static_assert(CS_COUNT == 7, ...)` beside the enum.
[`viewportImages`](../../src/pages/shuttle_view.cpp#L34) is a 14-element positional array
indexed by state; its own comment already says *"this won't do - image depends on location
properties e.g. station presence... or location type"*. That is now exactly right and
expressible: the image is a function of (activity, place), so it keys off `state()` plus
`docked()` / `inOrbit()` rather than one flattened enum.

**Unchanged:** `overlay.h/.cpp`, `ui_elements.h`, `autopilot_view.cpp`.

---

## Migration order

**The order was inverted in practice: 6 and 7 went first.** That was the right call — the
plan put "make it private" last so the compiler would produce the list of missed sites, and
that is exactly how the collapse was driven. Doing it early meant every later step inherits a
build that already fails at each stale site. What follows is the state as built.

| Step | Status |
|---|---|
| 1. `craft_action.h` / `.cpp`, `craft_type.h`, predicates | **done** |
| 2. `checkCapability` / `checkAction` / `perform`; `craftCanDock` → `checkCraftCanDock` | **not started** |
| 3. Countdown into `Craft::update`; delete `Shuttle::update` / `IOS::update`; `CC_BOARDING` gate | **not started** |
| 4. Autopilot onto the verbs | **partly** — it calls them, but `engageAutopilot` still returns `bool` and `Craft::update` still gates on `drive` |
| 5. `ShuttleView` control table + keyboard | **not started** — per-key `if` blocks, though they now call verbs rather than writing state |
| 6. Private state machine | **done** (`protected`, so subclasses still reach it) |
| 7. Collapse the enum to seven values | **done** |
| 8. Polish | `bay_view` done; `architecture.md` still lists the old enum |

### What the inversion left undone

**The capability tier is dead code.** `craftCapabilities` is defined in
[craft_action.cpp](../../src/state/craft_action.cpp) and consumed by **nothing** — there is
no `craftHasCapability` call anywhere. At the same time every `type == CT_SHUTTLE` guard was
removed on the way through, so the rules that table exists to state are currently enforced
nowhere:

- A shuttle can engage its interplanetary drive. `KEY_E` calls `engageDrive()` unguarded
  ([shuttle_view.cpp:202](../../src/pages/shuttle_view.cpp#L202)), and the autopilot calls it
  for any endpoint at a different body — so a shuttle given a cross-body route will fly it.
- An IOS can ascend or descend if it is ever on a surface; only position, not capability,
  stops it.
- `onSpacecraftDocked` captures hostile orbitals for **any** craft — the `CC_BOARDING` gate
  that was to keep shuttle behaviour unchanged is not there.

This is the real cost of taking 6/7 before 2. Nothing is *worse* than before — those guards
were scattered `type ==` tests that the plan was replacing anyway — but the replacement has
not landed, so the interval is genuinely unguarded.

**Step 3 is now the highest-value remaining step**, and more so than when written.
`Shuttle::update` and `IOS::update` still carry near-identical countdown loops, and the
duplication has already cost: the `state = CS_IDLE` before `switch (state)` bug that made
every timed transition dead was written **twice**, once in each copy, because they are
copies. They have legitimately diverged now (the shuttle chains `CS_LAUNCHING` into
`CS_ASCENDING`; the IOS cannot), so unifying means one switch whose arms are guarded by
capability rather than by which subclass compiled it.

### Remaining work, in order

1. **Unify the countdown** into `Craft::update`; delete both subclass overrides. Gate the
   surface arms on `CC_ATMOSPHERIC` and the transit arm on `CC_INTERPLANETARY`, which is the
   first real consumer of the capability table.
2. **Add the guard layer** (`checkCapability` / `checkAction` / `perform`) and move the
   verbs' ad-hoc situation checks into it — `dock()` testing `location->type ==
   LOCATION_TYPE_ORBIT` is the situation tier written inline.
3. **`craftCanDock` → `checkCraftCanDock`** returning a reason rather than a bool.
4. **Autopilot and `ShuttleView` onto `perform`**, so a refused action reports why instead of
   silently no-opping. `engageAutopilot` returns `CraftActionResult`.
5. **Polish:** `architecture.md:90-105` still lists the pre-collapse enum.

### Open questions surfaced during implementation

- **`ascend()` accepts a docked start** (`isOnSurface()` includes surface facilities) while
  `dock()` and `descend()` require the bare region. So a craft can climb straight out of a
  station without launching, and `docked()` stays true for the whole ascent — which
  contradicts the `moving() && docked()` invariant asserted in
  [test_facility_location.cpp](../../tests/test_facility_location.cpp). Either the shortcut
  is intended and the test should stop asserting that, or `ascend()` should require
  `LOCATION_TYPE_SURFACE`.
- **Two redundant fixes for the lost ascent.** The launch→ascend chain lives both in the
  shuttle expiry switch and in the autopilot's ascend branch; either alone is sufficient
  (verified). Kept both: the expiry arm serves a manual launch, the autopilot branch serves a
  route. Worth collapsing to one if step 3 unifies the expiry switch.
- **`dock()` still writes `state` and `state_timer` raw**, leaving `total_state_timer` stale,
  so `stateProgress()` misreads during an approach. The other three verbs use
  `setTimedState`.
- **`createShuttle` leaves `name` empty**, so autopilot log lines read "Autopilot:  ...".
  `createIOS` names its craft `IOS-%04d`.

## Verification

`./reconf.sh && make tests && ./bin/Debug/tests --test-case="*Craft*"` — a new test file
is invisible to the generated makefiles without the reconf.

`tests/test_craft_state.cpp` needs no window: `Game::createCurrent()` already builds a
`LinearTransitTimeCalculator`, and `createSystem` / `createLocation` / `createShuttle` give a
fixture in four lines. The "for every `CraftState`" sweeps place a craft with
`restoreState(s, 0, 0)` — the same sanctioned bypass the loader uses.

1. **Guard/mutator agreement.** For every `CraftState` × `CraftAction` × craft type:
   `auto expect = craft->checkAction(a); CHECK(craft->perform(a) == expect);` and when
   refused, `state()` / `stateTimer()` / `progress()` are unchanged. `perform` must return
   *the same reason*, not merely agree on accept/reject. Plus the fail-safe default:
   `CraftActionResult r; CHECK(!r); CHECK(r == CAC_UNKNOWN);`
2. **Predicate invariants.** `moving()` is true exactly for the states an expiry transition
   leads out of; `moving()` and `docked()` never overlap (they would deadlock undocking);
   from `CS_WORKING` every action is `CAC_BUSY` except `CA_CANCEL_WORK`. `progress()` is in
   `[0,1]` and `0` in every untimed state. And the invariant the collapse buys: for every
   reachable craft, `docked() == location->isFacility()` — assert it, because it is now a
   tautology and should stay one.
3. **Capability tier.** `shuttle->checkAction(CA_ENGAGE_DRIVE) == CAC_NOT_CAPABLE` from
   every state × place combination, and no command sequence reaches `CS_TRANSIT`. An IOS
   refuses both `CA_DESCEND` and `CA_ASCEND`.
4. **Fitment tier.** With `drive == false`, from states that would otherwise permit them,
   `CA_DOCK` / `CA_LAUNCH` / `CA_ASCEND` / `CA_DESCEND` all return `CAC_NO_DRIVE` (not
   `CAC_WRONG_STATE` — tier order matters); `CA_WORK` is still accepted while docked;
   `engageAutopilot()` returns `CAC_NO_DRIVE` and leaves `autopilot->state` alone. Setting
   `drive = true` permits all four.
5. **Situation tier.** No orbital → `CAC_NO_ORBITAL`; not operational →
   `CAC_ORBITAL_INCOMPLETE`; hostile with drones → `CAC_DEFENDED`; hostile with none →
   `CAC_OK`.
6. **Round trip**, asserting *both* halves at each step — the state and the location,
   since they are now one fact held in one place. Docked at a surface station →
   `launch()` → `CS_LAUNCHING` at the surface region → `CS_ASCENDING` (timer `CSTD_ASCENT`)
   → `CS_IDLE` at the orbit region; then `descend()` → `CS_IDLE` at the station with a
   `ResourceFacility` present, at the bare surface region without. This is the existing
   "a craft's location matches what it is doing" case in
   [test_facility_location.cpp](../../tests/test_facility_location.cpp), extended.
7. **Capture.** An IOS docking at a hostile undefended orbital flips `faction_id`; a shuttle
   in the same setup does not.
8. **Autopilot.** At destination with a non-operational orbital, one tick disengages rather
   than retrying forever. A shuttle with endpoints at two different *bodies* fails
   `engageAutopilot()` with `CAC_NOT_CAPABLE`; between two facilities at one body it never
   engages its drive.
9. **Save/load.** Existing round-trip cases in `test_db.cpp` pass untouched — they are the
   enum-ordering regression suite. Add one writing `state_timer = 1.5, total = 0` directly
   into the DB and asserting `restoreState` repairs the total.

**Enforcement.** Steps 1-5 rely on a grep; after step 6 the compiler enforces it.

```bash
# steps 1-5: expect no output
grep -rn "state = CS_\|state_timer = " src include \
  | grep -v "src/state/craft.cpp\|include/state/craft.h\|src/loaders/"

# after step 6: the only sanctioned bypass. Expect loader.cpp and main.cpp only.
grep -rn "restoreState" src include
```

**Play-test** after step 5: dock / undock / ascend / descend on a shuttle and an IOS, with
and without an orbital present, by mouse *and* keyboard, confirming the paths now agree
(today `A` lets an IOS descend while the button does not). An IOS should offer neither
ascend nor descend. F5/F8 should round-trip a craft mid-manoeuvre.

**Driveless craft become immobile — expected, with no in-game recovery yet.**
`resources/initial.db`'s `craft` table is empty and `main.cpp` gives every scaffold craft
`drive = true`, so a fresh game is unaffected. But
[`commissionShuttle`](../../src/state/game.cpp#L210) builds the hull from an `S_Chassis`
alone and fits a drive only `if (facility->stores.items[ItemType::S_Drive] > 0)`, and
nothing else ever assigns `craft->drive` —
[bay_view.cpp:534](../../src/pages/bay_view.cpp#L534) only displays it. Commission a shuttle
with an empty drive store during the play-test and confirm it reads as an unfinished hull
rather than a bug.

---

## Deferred

- **"Install drive" in the shuttle bay.** The recovery path for the case above, and the home
  for damaged drives later. Deliberately *not* a `CraftAction`: single initiator (the
  player, at the bay), no autonomous path that could race it, no state transition or timer.
  Code it directly in `BayView`, in the style of `canCommissionShuttle` / `commissionShuttle`.
- **Hover-with-reason on refused controls.** Add `bool enabled = true` to
  [`Overlay::renderButton`](../../include/pages/overlay.h#L30) and show `result.text()`. Two
  gotchas: the body must save/restore `GuiGetState()` rather than call `GuiEnable()`, because
  [bay_view.cpp:340-362](../../src/pages/bay_view.cpp#L340-L362) already wraps its call in
  `GuiDisable()`; and [`UITransparentButtonState`](../../include/assets/ui_elements.h#L164)
  needs `*_COLOR_DISABLED = 0x00000000`, or a disabled cockpit hotspot paints raygui's dark
  box over the artwork.
- **~~Decompose `CraftState` to match `Endpoint`.~~ Promoted into [§2](#2-what-orbit-as-location-does-to-the-enum),
  and answered differently than this predicted.** The reasoning was right — 14 values are a
  product of position × docked × activity, and `Endpoint` modelled that explicitly while the
  craft's own position was one overloaded enum. The predicted fix was to give `Craft`
  matching *fields*, and the objection was that fields need their own rules against invalid
  combinations that a single enum gives free.
  Orbit-as-location dissolved both. The second and third dimensions did not become fields on
  `Craft`; they became the craft's `location`, which is a real place in a hierarchy. Nothing
  can hold an invalid combination because there is only one record of it, and `Endpoint`
  collapsed to a bare `Location *` from the same direction. The lesson worth keeping: the
  duplication was a symptom of a missing *domain* concept, not of a missing struct.

- **A `StateTimer` value type.** `state_timer_` and `total_state_timer_` now have one writer
  whose whole job is keeping them in step; a ~12-line type (`set()` writes both, `tick()`
  absorbs the countdown, `progress()` moves onto it) would make desync unrepresentable
  rather than merely conventional.
- **`AS_DISABLED` and `AS_COMPLETE`** are both defined and never assigned. Giving them
  meaning is autopilot policy, not state model.

### Where this pattern applies again

Reach for `<Thing>Action` / `<Thing>ActionResult` when an operation can be requested by the
player *and* initiated autonomously by the simulation, with the two interleaving
non-deterministically — one shared, consistently applied permission check is then the only
way to keep them in step. The second driver is volume: too many control points to hold
consistent by hand. Factories qualify. A single-initiator operation does not.
