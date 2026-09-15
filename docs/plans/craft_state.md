# Craft state model refactor

## Context

`CraftState` (14 values) is currently driven from four places that each re-implement the
same rules differently: the expiry `switch` in [shuttle.cpp:14-68](../../src/state/shuttle.cpp#L14-L68),
a divergent subset of it in [ios.cpp:9-42](../../src/state/ios.cpp#L9-L42), its own copy of
the dock/descend/transit rules in [autopilot.cpp:111-154](../../src/state/autopilot.cpp#L111-L154),
and two further copies in `ShuttleView` — mouse in `render()`, keyboard in `input()` — that
disagree with each other. Callers write `state = X; state_timer = Y;` directly, leaving
`total_state_timer` stale.

**Goal:** one timed-transition mechanism, and one permission check asked the same way by the
player, the autopilot and the game logic.

### The model

Three questions are conflated today into scattered `type == CT_SHUTTLE`, `if (drive)` and
`state == CS_X` tests. They are separated here:

| Tier | Question | Varies with | Example refusal |
|---|---|---|---|
| **Capability** | Is this hull *designed* to do it? | `CraftType`, permanent | a shuttle can never make an interplanetary transit |
| **Fitment** | Is the equipment *installed*? | per craft, changes in the bay | no drive fitted → cannot dock, undock, ascend, descend |
| **Situation** | Is it allowed *right now*? | state + world | not in orbit; orbital incomplete; defended by drones |

`drive` is the engine, so every manoeuvre needs it — not just transit. `CC_TRANSIT` is
separately whether the hull is *rated* to cross interplanetary space. A shuttle therefore
legitimately carries a drive (it needs engines to ascend and descend) while never being able
to transit.

**Not in scope:** no UI is built to display a refusal reason. A control whose action is
refused is simply not drawn, as `ShuttleView` already does. The reason is still carried
through the guards — it drives logging, and leaves the door open. See [Deferred](#deferred).

---

## 1. `include/state/craft_action.h` (new)

Free of `game.h`, so it is unit-testable without a `Game`.

```cpp
#pragma once

#include <cstdint>
#include "state/craft.h"

// ---------------------------------------------------------------- actions
// What someone is asking the craft to do. Runtime only, never persisted.
enum CraftAction : uint8_t
{
    CA_NONE,
    CA_DOCK,
    CA_UNDOCK,
    CA_DESCEND,
    CA_ASCEND,
    CA_ENGAGE_DRIVE,
    CA_WORK,
    CA_CANCEL_WORK,
    CA_COUNT
};
extern const char *craftActionNames[CA_COUNT];

// ------------------------------------------------------------ capability
// What this hull is DESIGNED to do. Per CraftType, permanent.
enum CraftCapability : uint16_t
{
    CC_NONE       = 0,
    CC_DOCK_ORBIT = 1 << 0, // can mate with an orbital
    CC_SURFACE    = 1 << 1, // rated for ascent / descent / landing
    CC_TRANSIT    = 1 << 2, // rated for interplanetary transit
    CC_CAPTURE    = 1 << 3, // docking at a hostile orbital captures it
};

// Shuttle: surface <-> orbit at one location. IOS/SCG: orbit-to-orbit, may capture.
extern const uint16_t craftCapabilities[CT_COUNT];

inline bool craftHasCapability(CraftType t, uint16_t bits)
{
    return (craftCapabilities[t] & bits) == bits;
}

// -------------------------------------------------------------- fitment
// Equipment an action needs FITTED on this particular craft. `drive` is the
// engine, so every manoeuvre needs it. Only the work actions, which happen with
// the craft already made fast, do not.
extern const bool actionNeedsDrive[CA_COUNT];

// --------------------------------------------------------------- result
// The outcome of REQUESTING an action. CAC_UNKNOWN is 0 so a value-initialised
// or memset result reads as "refused, no reason recorded" -- it fails safe and
// is never returned deliberately.
enum CraftActionCode : uint8_t
{
    CAC_UNKNOWN = 0,
    CAC_OK,                 // accepted -- the manoeuvre has STARTED, not finished
    CAC_NOT_CAPABLE,        // capability
    CAC_NO_DRIVE,           // fitment
    CAC_NO_SUPPLY_POD,      // fitment (autopilot)
    CAC_WRONG_STATE,        // situation
    CAC_BUSY,
    CAC_NO_ORBITAL,
    CAC_ORBITAL_INCOMPLETE,
    CAC_DEFENDED,
    CAC_NO_DESTINATION,
    CAC_ROUTE_UNREACHABLE,
    CAC_COUNT
};
extern const char *craftActionCodeText[CAC_COUNT]; // "No drive fitted", ...

// What every request returns. The explicit operator bool is the point: a caller
// writes `if (craft->dock())` and cannot get the polarity wrong, and cannot
// silently assign or compare it to an int either.
class CraftActionResult
{
    CraftActionCode code_{CAC_UNKNOWN};

public:
    constexpr CraftActionResult() = default;
    constexpr CraftActionResult(CraftActionCode c) : code_{c} {} // implicit: `return CAC_NO_DRIVE;`

    constexpr explicit operator bool() const { return code_ == CAC_OK; }
    constexpr CraftActionCode code() const { return code_; }
    const char *text() const { return craftActionCodeText[code_]; }

    friend constexpr bool operator==(CraftActionResult a, CraftActionResult b)
    {
        return a.code_ == b.code_;
    }
};
```

```cpp
// src/state/craft_action.cpp

#include "state/craft_action.h"

const char *craftActionNames[CA_COUNT] = {
    "None", "Dock", "Undock", "Descend", "Ascend",
    "Engage drive", "Work", "Cancel work"};

const uint16_t craftCapabilities[CT_COUNT] = {
    /* CT_SHUTTLE */ CC_DOCK_ORBIT | CC_SURFACE,
    /* CT_IOS     */ CC_DOCK_ORBIT | CC_TRANSIT | CC_CAPTURE,
    /* CT_SCG     */ CC_DOCK_ORBIT | CC_TRANSIT | CC_CAPTURE,
};

const bool actionNeedsDrive[CA_COUNT] = {
    /* CA_NONE         */ false,
    /* CA_DOCK         */ true,
    /* CA_UNDOCK       */ true,
    /* CA_DESCEND      */ true,
    /* CA_ASCEND       */ true,
    /* CA_ENGAGE_DRIVE */ true,
    /* CA_WORK         */ false,
    /* CA_CANCEL_WORK  */ false,
};

const char *craftActionCodeText[CAC_COUNT] = {
    "Unknown",
    "OK",
    "Not capable",
    "No drive fitted",
    "No supply pod fitted",
    "Not possible from here",
    "Manoeuvre in progress",
    "No orbital station here",
    "Orbital station incomplete",
    "Defended by drones",
    "No destination set",
    "Route not flyable by this craft",
};
```

**Newly enforced.** Three rules previously held only by being awkward to violate:

- An IOS cannot descend, therefore cannot ascend — both need `CC_SURFACE`. Ascent was never
  checked anywhere before.
- A shuttle cannot ascend or descend without a drive.
- Docking and undocking need a drive.

`CA_UNDOCK` needing a drive means a craft whose drive is removed or damaged is stranded at
its station. That is the intended reading, and the hook for damaged drives later.

---

## 2. State predicates (`craft.h` / `craft.cpp`)

`CraftState`'s 14 values are a product of position × docked × activity. Rather than repeat
`state == CS_X || state == CS_Y` at each site, name the dimensions:

```cpp
    // Which half of the Endpoint triple the craft is currently at.
    // SLOC_COUNT means "between" -- ascending, descending, in transit.
    SublocationType sublocation() const;

    bool docked() const;   // made fast to a facility
    bool working() const;  // pods tick in this state
    bool moving() const;   // drive is running; no new command is accepted
```

Each is an exhaustive `switch` **with no `default:`** — verified on this toolchain that
clang emits `-Wswitch` by default (premake sets no extra warning level), so a 15th
`CraftState` produces a compiler warning at every predicate that has not considered it. A
lookup table would instead give a silent zero for the missing row.

```cpp
// src/state/craft.cpp

SublocationType Craft::sublocation() const
{
    switch (state_)
    {
    case CS_SURFACE:
    case CS_SURFACE_DOCKED:
    case CS_SURFACE_DOCK_WORK:
    case CS_SURFACE_WORK:
    case CS_SURFACE_LAUNCH:
        return SLOC_SURFACE;
    case CS_ORBIT:
    case CS_ORBIT_DOCKING:
    case CS_ORBIT_DOCKED:
    case CS_ORBIT_DOCK_WORK:
    case CS_ORBIT_WORK:
    case CS_ORBIT_LAUNCH:
        return SLOC_ORBIT;
    case CS_ASCENDING:
    case CS_DESCENDING:
    case CS_TRANSIT:
    case CS_COUNT:
        return SLOC_COUNT; // between
    }
    return SLOC_COUNT;
}

bool Craft::docked() const
{
    switch (state_)
    {
    case CS_SURFACE_DOCKED:
    case CS_SURFACE_DOCK_WORK:
    case CS_ORBIT_DOCKED:
    case CS_ORBIT_DOCK_WORK:
        return true;
    case CS_SURFACE:
    case CS_SURFACE_WORK:
    case CS_SURFACE_LAUNCH:
    case CS_ASCENDING:
    case CS_ORBIT:
    case CS_ORBIT_DOCKING:
    case CS_ORBIT_WORK:
    case CS_ORBIT_LAUNCH:
    case CS_DESCENDING:
    case CS_TRANSIT:
    case CS_COUNT:
        return false;
    }
    return false;
}

bool Craft::working() const
{
    switch (state_)
    {
    case CS_SURFACE_DOCK_WORK:
    case CS_SURFACE_WORK:
    case CS_ORBIT_DOCK_WORK:
    case CS_ORBIT_WORK:
        return true;
    case CS_SURFACE:
    case CS_SURFACE_DOCKED:
    case CS_SURFACE_LAUNCH:
    case CS_ASCENDING:
    case CS_ORBIT:
    case CS_ORBIT_DOCKING:
    case CS_ORBIT_DOCKED:
    case CS_ORBIT_LAUNCH:
    case CS_DESCENDING:
    case CS_TRANSIT:
    case CS_COUNT:
        return false;
    }
    return false;
}

bool Craft::moving() const
{
    switch (state_)
    {
    case CS_SURFACE_LAUNCH:
    case CS_ASCENDING:
    case CS_ORBIT_DOCKING:
    case CS_ORBIT_LAUNCH:
    case CS_DESCENDING:
    case CS_TRANSIT:
        return true;
    case CS_SURFACE:
    case CS_SURFACE_DOCKED:
    case CS_SURFACE_DOCK_WORK:
    case CS_SURFACE_WORK:
    case CS_ORBIT:
    case CS_ORBIT_DOCKED:
    case CS_ORBIT_DOCK_WORK:
    case CS_ORBIT_WORK:
    case CS_COUNT:
        return false;
    }
    return false;
}
```

`atEndpoint()` then collapses to a direct comparison against the triple
[`Endpoint`](../../include/state/waypoint.h) already holds:

```cpp
    inline bool atEndpoint() const
    {
        const Endpoint &d = currentDestination();
        return d.location == location
            && d.sublocation == sublocation()
            && d.docked == docked();
    }
```

Two deliberate semantics: `working()` includes `CS_SURFACE_WORK` / `CS_ORBIT_WORK`, so pods
tick in the working-but-not-docked case (building in orbit) — a widening of
[craft.cpp:106](../../src/state/craft.cpp#L106), safe because neither state is reachable
today. And `moving()` means the drive is running, which is *why* commands are refused.

---

## 3. `Craft`: private state, one guard, one mutator

```cpp
class Craft
{
    // The state machine. Nothing outside Craft may assign these.
    CraftState state_{CS_ORBIT_DOCKED};
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
    inline CraftActionResult undock()      { return perform(CA_UNDOCK); }
    inline CraftActionResult descend()     { return perform(CA_DESCEND); }
    inline CraftActionResult ascend()      { return perform(CA_ASCEND); }
    inline CraftActionResult work(float d) { return perform(CA_WORK, d); }

    CraftActionResult engageAutopilot();
    void disengageAutopilot();

    SublocationType sublocation() const;
    bool docked() const;
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
    // CA_ASCEND needs CC_SURFACE just as CA_DESCEND does: a craft that cannot come
    // down has no business going up. CA_UNDOCK is CC_NONE -- it serves both the
    // orbital and the surface dock, and the state tier requires you to be in one.
    static const uint16_t needs[CA_COUNT] = {
        /* CA_NONE         */ CC_NONE,
        /* CA_DOCK         */ CC_DOCK_ORBIT,
        /* CA_UNDOCK       */ CC_NONE,
        /* CA_DESCEND      */ CC_SURFACE,
        /* CA_ASCEND       */ CC_SURFACE,
        /* CA_ENGAGE_DRIVE */ CC_TRANSIT,
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
        if (state_ != CS_ORBIT) { return CAC_WRONG_STATE; }
        return Game::getCurrent()->checkCraftCanDock(this);

    case CA_UNDOCK:
        return docked() ? CAC_OK : CAC_WRONG_STATE;

    case CA_DESCEND:
        return (state_ == CS_ORBIT) ? CAC_OK : CAC_WRONG_STATE;

    case CA_ASCEND:
        return (sublocation() == SLOC_SURFACE) ? CAC_OK : CAC_WRONG_STATE;

    case CA_ENGAGE_DRIVE:
    {
        if (docked())               { return CAC_WRONG_STATE; }
        const Endpoint &d = currentDestination();
        if (!d.location)            { return CAC_NO_DESTINATION; }
        if (d.location == location) { return CAC_WRONG_STATE; }
        return CAC_OK;
    }

    case CA_WORK:
        return docked() ? CAC_OK : CAC_WRONG_STATE;

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
        setTimedState(CS_ORBIT_DOCKING, CSTD_DOCK);
        break;
    case CA_UNDOCK:
        setTimedState(sublocation() == SLOC_ORBIT ? CS_ORBIT_LAUNCH : CS_SURFACE_LAUNCH,
                      CSTD_LAUNCH);
        break;
    case CA_DESCEND:
        setTimedState(CS_DESCENDING, CSTD_DESCENT);
        break;
    case CA_ASCEND:
        setTimedState(CS_SURFACE_LAUNCH, CSTD_LAUNCH);
        break;
    case CA_ENGAGE_DRIVE:
        if (!beginTransit()) { return CAC_ROUTE_UNREACHABLE; }
        break;
    case CA_WORK:
        setTimedState(sublocation() == SLOC_ORBIT ? CS_ORBIT_DOCK_WORK : CS_SURFACE_DOCK_WORK,
                      duration > 0.0f ? duration : 1.0f);
        break;
    case CA_CANCEL_WORK:
        setTimedState(sublocation() == SLOC_ORBIT ? CS_ORBIT_DOCKED : CS_SURFACE_DOCKED);
        break;
    case CA_NONE:
    case CA_COUNT:
        return CAC_WRONG_STATE;
    }
    return CAC_OK;
}

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
    case CS_SURFACE_LAUNCH:
        setTimedState(CS_ASCENDING, CSTD_ASCENT);
        break;

    case CS_ASCENDING:
    case CS_ORBIT_LAUNCH:
    case CS_ORBIT_WORK:
        setTimedState(CS_ORBIT);
        break;

    case CS_SURFACE_WORK:
        // worked OUTSIDE a dock, so it ends on the surface, not docked
        setTimedState(CS_SURFACE);
        break;

    case CS_SURFACE_DOCK_WORK:
        setTimedState(CS_SURFACE_DOCKED);
        onDockWorkComplete();
        break;

    case CS_ORBIT_DOCK_WORK:
        setTimedState(CS_ORBIT_DOCKED);
        onDockWorkComplete();
        break;

    case CS_ORBIT_DOCKING:
        setTimedState(CS_ORBIT_DOCKED);
        onDocked();
        game->onSpacecraftDocked(this);   // was IOS-only
        break;

    case CS_DESCENDING:
        if (game->resourceFacilityAt(location))
        {
            setTimedState(CS_SURFACE_DOCKED);
            onDocked();
        }
        else
        {
            setTimedState(CS_SURFACE);
        }
        break;

    case CS_TRANSIT:                      // was IOS-only; a shuttle hung here forever
        setTimedState(CS_ORBIT);
        arriveAtLocation();
        game->onSpacecraftArrival(this);
        break;

    // stable states: a timer should not have been running
    case CS_SURFACE:
    case CS_SURFACE_DOCKED:
    case CS_ORBIT:
    case CS_ORBIT_DOCKED:
    case CS_COUNT:
        break;
    }
}
```

Unifying the two switches is safe because capabilities make the extra arms unreachable per
type: a shuttle cannot enter `CS_TRANSIT`, an IOS cannot enter the surface states.
`onSpacecraftDocked` now fires for every craft; `Game::onSpacecraftDocked` gates the capture
itself on `CC_CAPTURE`, so shuttle behaviour is unchanged and the rule lives in one place:

```cpp
void Game::onSpacecraftDocked(Craft *craft)
{
    if (!craftHasCapability(craft->type, CC_CAPTURE))    { return; }
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
    if (state < AS_ON)               { return; }
    if (craft->state() != CS_ORBIT)  { return; }

    const Endpoint &dest = craft->currentDestination();

    if (dest.location != craft->location)
    {
        CraftActionResult r = craft->perform(CA_ENGAGE_DRIVE);
        if (!r)
        {
            TraceLog(LOG_WARNING, "Autopilot: %s cannot depart - %s", craft->name, r.text());
            state = AS_OFF;
        }
    }
    else if (dest.sublocation == SLOC_ORBIT && dest.docked)
    {
        if (!craft->dock())
        {
            // covers non-operational and drone-defended, not just "no station".
            // Previously this only cleared `docked` when there was no station at
            // all, so it retried forever against a defended one.
            craft->destinations[craft->destination_index].docked = false;
        }
    }
    else if (dest.sublocation == SLOC_SURFACE && dest.docked)
    {
        craft->descend();   // gains the CC_SURFACE guard the mouse path had as `type ==`
    }
}

void Autopilot::onDockWorkComplete(Craft *craft)
{
    craft->undock();
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

    if (a.location != b.location) { needed |= 1u << CA_ENGAGE_DRIVE; }

    for (const Endpoint &e : destinations)
    {
        if (e.sublocation == SLOC_SURFACE)
        {
            needed |= (1u << CA_DESCEND) | (1u << CA_ASCEND);
        }
        if (e.docked)
        {
            needed |= (1u << CA_DOCK) | (1u << CA_UNDOCK);
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

    for (int i = 0; i < MAX_DESTINATIONS; ++i)
    {
        if (destinations[i].location) { destinations[i].docked = true; }
    }
    if (docked()) { undock(); }

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
    {CA_UNDOCK,  {561, 838, 72, 52}, "Undock"},   // same hotspot; states are exclusive
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
    {KEY_D, {CA_UNDOCK, CA_DOCK}},      // guards are mutually exclusive
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
| [include/state/game.h](../../include/state/game.h), [src/state/game.cpp](../../src/state/game.cpp) | `craftCanDock` → `checkCraftCanDock`; `onSpacecraftDocked` gates on `CC_CAPTURE`; `activatePod` / `updateActivePod` use `perform(CA_WORK)` / `perform(CA_CANCEL_WORK)` |
| [src/state/autopilot.cpp](../../src/state/autopilot.cpp) | §5 |
| [include/pages/shuttle_view.h](../../include/pages/shuttle_view.h), [src/pages/shuttle_view.cpp](../../src/pages/shuttle_view.cpp) | §6; delete `craft_can_dock`; `progress()` at :283 |
| [src/pages/bay_view.cpp](../../src/pages/bay_view.cpp) | docked tests → `docked() && sublocation() == facility->sublocation` |
| [src/orrery.cpp](../../src/orrery.cpp) | :155 → `craft->progress()` (absorbs the divide-by-zero guard) |
| [src/loaders/loader.cpp](../../src/loaders/loader.cpp) | direct writes → one `restoreState(...)` call |
| [src/loaders/save_game.cpp](../../src/loaders/save_game.cpp) | reads → `state()` / `stateTimer()` |
| [src/main.cpp](../../src/main.cpp), [src/pages/master_control.cpp](../../src/pages/master_control.cpp), [src/pages/drone_control_view.cpp](../../src/pages/drone_control_view.cpp), [tests/test_db.cpp](../../tests/test_db.cpp) | mechanical `->state` → `->state()`; scaffold writes → `restoreState` |
| [architecture.md](../../architecture.md) | :90-105 lists a stale 12-value enum |

**Unchanged:** the SQLite schema and `CraftState` ordering (`CS_COUNT` stays 14, nothing
reordered, so existing saves load identically); `overlay.h/.cpp`, `ui_elements.h`,
`autopilot_view.cpp`.

Add `static_assert(CS_COUNT == 14, ...)` beside the enum, and one in `shuttle_view.cpp` for
the 14-element positional `viewportImages` array, which would silently desync today.

---

## Migration order

`make && make tests && ./bin/Debug/tests` after each step; each reverts alone.

1. **Add `craft_action.h` / `.cpp`, the predicates, and the static_asserts.** Nothing
   consumes them yet. `./reconf.sh` — premake globs `src/**.cpp` at configure time.
2. **Add `checkCapability` / `checkAction` / `perform` / verbs**; route `launch`, `work`,
   `engageDrive` through them; `craftCanDock` → `checkCraftCanDock`. Existing callers keep
   working; the only observable change is that `total_state_timer` becomes correct. Do
   *not* move the countdown yet — it would double-tick against the subclass loops.
3. **Move the countdown into `Craft::update`, delete `Shuttle::update` / `IOS::update`**,
   adding the `CC_CAPTURE` gate in `onSpacecraftDocked` in the same commit so shuttle
   behaviour is unchanged. Highest-value step.
4. **Autopilot onto the verbs**; `engageAutopilot` returns `CraftActionResult`; gate becomes
   `autopilot->state >= AS_ON`. Own commit — the one real behaviour change.
5. **`ShuttleView` control table + keyboard.** Play-test.
6. **Make the state machine private (§3).** Deliberately last: the build now fails at every
   site the earlier steps missed, so the compiler produces the list.
7. **Polish:** `bay_view` predicates, `architecture.md`.

---

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
   from `CS_ORBIT_DOCK_WORK` every action is `CAC_BUSY` except `CA_CANCEL_WORK`.
   `progress()` is in `[0,1]` and is `0` in every untimed state.
3. **Capability tier.** `shuttle->checkAction(CA_ENGAGE_DRIVE) == CAC_NOT_CAPABLE` from all
   14 states, and no command sequence reaches `CS_TRANSIT`. An IOS refuses both
   `CA_DESCEND` and `CA_ASCEND`.
4. **Fitment tier.** With `drive == false`, from states that would otherwise permit them,
   `CA_DOCK` / `CA_UNDOCK` / `CA_ASCEND` / `CA_DESCEND` all return `CAC_NO_DRIVE` (not
   `CAC_WRONG_STATE` — tier order matters); `CA_WORK` is still accepted while docked;
   `engageAutopilot()` returns `CAC_NO_DRIVE` and leaves `autopilot->state` alone. Setting
   `drive = true` permits all four.
5. **Situation tier.** No orbital → `CAC_NO_ORBITAL`; not operational →
   `CAC_ORBITAL_INCOMPLETE`; hostile with drones → `CAC_DEFENDED`; hostile with none →
   `CAC_OK`.
6. **Round trip.** `CS_SURFACE_DOCKED` → `ascend()` → `CS_SURFACE_LAUNCH` → `CS_ASCENDING`
   (timer `CSTD_ASCENT`) → `CS_ORBIT`; then `descend()` → `CS_SURFACE_DOCKED` with a
   `ResourceFacility` present, `CS_SURFACE` without.
7. **Capture.** An IOS docking at a hostile undefended orbital flips `faction_id`; a shuttle
   in the same setup does not.
8. **Autopilot.** At destination with a non-operational orbital, one tick leaves `CS_ORBIT`
   and clears `dest.docked` rather than retrying forever. A shuttle with endpoints at
   different locations fails `engageAutopilot()` with `CAC_NOT_CAPABLE`.
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
- **Decompose `CraftState` to match `Endpoint`.** The 14 values are a product of position ×
  docked × activity, and [`Endpoint`](../../include/state/waypoint.h) already models exactly
  that as `{location, sublocation, docked}` — so a craft's destination is decomposed while
  its current position is one overloaded enum, and `atEndpoint()` exists only to bridge them.
  Fields would make combinations like "working, in orbit, not docked" expressible without a
  state per case, but `craft.state` is a persisted int column, `viewportImages[CS_COUNT]` is
  indexed positionally by it, and a field model needs its own rules against invalid
  combinations that the single enum gives free. §2's predicates are the cheap step: they name
  the dimensions and put every consumer behind those names, so the decomposition later
  becomes a change of implementation rather than of every call site.
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
