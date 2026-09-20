# Craft actions: verbs, guards and UI integration

## Purpose

- **Ask one question in one place.** Whether a craft may dock, launch, ascend, descend or
  engage its drive is decided by the craft, not re-derived by each caller.
- **Verbs, not state assignment.** Callers say `craft->ascend()`; nothing outside `Craft`
  writes `state` or the timers.
- **Three tiers of refusal**, asked in order so the reason returned is the most specific:

  | Tier | Question | Varies with |
  |---|---|---|
  | **Capability** | Is this hull *designed* to do it? | `CraftType`, permanent |
  | **Fitment** | Is the equipment *installed*? | per craft, changes in the bay |
  | **Situation** | Is it allowed *right now*? | position + world |

- **Position is not state.** Since orbit and surface became locations, `docked()` and
  `inOrbit()` read `craft->location` and cannot disagree with where the craft actually is.
  `CraftState` carries activity only.

---

## What exists now

**Delivered.**

- [craft_action.h](../../include/state/craft_action.h) / [.cpp](../../src/state/craft_action.cpp) —
  `CraftAction`, `CraftCapability`, `CraftActionCode`, `CraftActionResult`. `CraftType` lives
  in [craft_type.h](../../include/state/craft_type.h) so `craft.h` and `craft_action.h` can
  include each other's needs without a cycle.
- **Capabilities**: `CC_ATMOSPHERIC` / `CC_INTERPLANETARY` / `CC_INTERSTELLAR` /
  `CC_BOARDING`. Every hull docks, so docking is capability-free. `Craft::hasCapability()`
  wraps the table.
- **Seven-value `CraftState`**: `CS_IDLE`, `CS_WORKING`, `CS_LAUNCHING`, `CS_ASCENDING`,
  `CS_DESCENDING`, `CS_DOCKING`, `CS_TRANSIT`. Fields are `protected`; `setTimedState` /
  `setState` / `assignState` are the only writers, `currentState()` / `stateProgress()` the
  readers.
- **Predicates**: `docked()`, `inOrbit()`, `working()`, `moving()`, `inTransit()`,
  `isLaunching()`, `isDocking()`, `isWorking()`.
- **Verbs**: `dock()`, `launch()`, `ascend()`, `descend()`, `engageDrive()`,
  `disengageDrive()`, `work()`. Each carries its own guards inline and returns without
  effect when refused.
- **One update loop.** `Shuttle::update` and `IOS::update` are gone; `Craft::update` holds
  the countdown and a single expiry switch.
- **Capability enforcement** at nine sites, including `engageDrive()` (now refuses without
  `CC_INTERPLANETARY` or without `drive`) and `onSpacecraftDocked` (`CC_BOARDING`).

---

## The problem this plan now addresses

**A verb's guards are private to it, so the UI re-derives them — and the copies have
drifted.** A refused verb returns silently, so a wrong copy shows a control that does
nothing, or hides one that would work. Three live examples:

| Site | UI condition | The verb requires | Effect |
|---|---|---|---|
| [shuttle_view.cpp:398](../../src/pages/shuttle_view.cpp#L398) | `CC_ATMOSPHERIC && !location->isOnSurface()` | `CC_ATMOSPHERIC && location->isOnSurface()` | **Inverted.** The Ascend button shows only when ascending is impossible, and is hidden on the ground where it would work |
| [shuttle_view.cpp:383](../../src/pages/shuttle_view.cpp#L383) | `Game::craftCanDock()`, which tests `inOrbit()` | `location->type == LOCATION_TYPE_ORBIT` | `inOrbit()` is an ancestor test, so it is true while already docked. Dock and Undock then draw on the **same rectangle** at once |
| [shuttle_view.cpp:180](../../src/pages/shuttle_view.cpp#L180), [:207](../../src/pages/shuttle_view.cpp#L207) | `CC_INTERPLANETARY && drive` | plus `!docked()` | The drive control shows while docked; pressing it logs a refusal |

Each is the same failure: the question is answered twice and only one answer is authoritative.

---

## The shape: `canX()` returning a reason

Give every verb a const companion that answers the three tiers and returns *why*, and have
the verb itself call it. One implementation, two callers — the UI to decide what to draw,
the verb to decide whether to act.

```cpp
// craft.h
CraftActionResult canDock() const;
CraftActionResult canLaunch() const;
CraftActionResult canAscend() const;
CraftActionResult canDescend() const;
CraftActionResult canEngageDrive() const;
CraftActionResult canWork() const;
```

```cpp
// craft.cpp -- the guards move here out of the verb bodies, in tier order
CraftActionResult Craft::canAscend() const
{
    if (!hasCapability(CC_ATMOSPHERIC)) { return CAC_NOT_CAPABLE; }   // tier 1
    if (!drive)                         { return CAC_NO_DRIVE; }      // tier 2
    if (moving() || working())          { return CAC_BUSY; }          // tier 3
    if (!location->isOnSurface())       { return CAC_WRONG_STATE; }
    return CAC_OK;
}

Craft &Craft::ascend()
{
    if (!canAscend()) { return *this; }   // same question, one answer
    ...
}
```

`CraftActionResult` already has `explicit operator bool` and `text()`, so a call site cannot
get the polarity wrong and always has a string for the player.

### UI integration

Two levels, the second optional:

```cpp
// 1. control visibility -- replaces every hand-rolled condition
if (craft->canAscend() && overlay.renderButton(ascendButton, "", "Ascend to orbit", WHITE))
{
    craft->ascend();
}

// 2. show the reason instead of hiding the control
const CraftActionResult r = craft->canAscend();
if (overlay.renderButton(ascendButton, "", r ? "Ascend to orbit" : r.text(),
                         r ? WHITE : DISABLED_TINT) && r)
{
    craft->ascend();
}
```

Level 1 keeps today's behaviour — a refused control is simply not drawn — while removing the
duplicated logic. Level 2 is what makes the reasons visible, and is where
`CAC_ORBITAL_INCOMPLETE` or `CAC_DEFENDED` start earning their place.

---

## Status: delivered

All five steps are in, and the three UI bugs the plan existed to fix are gone.

- `canX()` for dock, launch, ascend, descend, engage-drive, work and engage-autopilot; every
  verb reduces to `if (canX())` plus the effect.
- `Game::craftCanDock` absorbed into `canDock()`, which reports `CAC_NO_ORBITAL` /
  `CAC_ORBITAL_INCOMPLETE` / `CAC_DEFENDED` rather than a bool.
- `ShuttleView` mouse and keyboard both ask the guards; the remaining `hasCapability` calls
  there are for *display* (which drive panel to draw), not permission.
- `engageAutopilot` / `disengageAutopilot` return `CraftActionResult`, and the autopilot
  button shows the refusal reason as its tooltip rather than vanishing.
- `Autopilot::update` asks the same guards — no raw capability tests remain in it.

Two things fell out that were not in the original shape:

- **Departure is level-triggered.** The autopilot launches from `update()` whenever the craft
  is docked and idle, so a refused launch is retried next tick. It used to fire once from an
  `onDockWorkComplete()` callback, which stranded any craft that could not go at that
  instant. Both that callback and the `ready` flag it needed are gone: `update()` already
  returns early on `working()`, so "docked and not working" *is* "work here is finished", and
  `CraftState` is persisted where a flag would have needed its own column.
- **`engageAutopilot` replays arrival through `Craft::onDocked()`**, not
  `Autopilot::onDocked()` directly, so engaging at a station keeps the `atEndpoint()` guard.
  Calling the autopilot's hook raw loaded pods from whichever endpoint `destination_index`
  named, which need not be the station the craft was sitting in.

### Still open

- **`CAC_BUSY` is never asserted directly.** The verb/guard equivalence sweep in
  `tests/test_craft_actions.cpp` uses idle craft only, so the busy tier is exercised
  incidentally by the cycle tests rather than pinned.
- **`disengageAutopilot` and `AS_COMPLETE` are untested.** `AS_DISABLED` is still never
  assigned anywhere, and `AS_COMPLETE` is treated as engaged by `state < AS_ON`.
- **Construction has no completion hook** — now its own plan, see
  [facility_construction.md](facility_construction.md). Removing `onDockWorkComplete` took the
  seam where a finished *build* would notify, and the `CS_WORKING` arm of the expiry switch
  carries the TODO marking the spot. A build finishing is a genuine one-shot event, unlike a
  departure, so it wants the hook rather than a condition re-tested each tick.

## Verification

`make && make tests && ./bin/Debug/tests` — 61 cases / 7411 assertions is the current
baseline. Covered by `tests/test_craft_actions.cpp` (**needs `./reconf.sh`** — premake globs at
configure time):

1. **Verb and guard agree.** For every craft type × place × state:
   `CHECK(bool(craft->canX()) == verbHadEffect(craft))`. A verb must never act when its
   `canX()` refuses, and never refuse when it accepts — that equivalence is the whole point.
2. **Tier order.** A craft with no drive on the ground returns `CAC_NO_DRIVE` from
   `canAscend()`, not `CAC_WRONG_STATE`; an IOS returns `CAC_NOT_CAPABLE` before either.
3. **The three UI bugs, as state assertions.** `canAscend()` is OK on the surface and refuses
   in orbit; `canDock()` refuses while already docked; `canEngageDrive()` refuses while
   docked.
4. **Fail-safe default.** `CraftActionResult r; CHECK(!r); CHECK(r == CAC_UNKNOWN);`

5. **A blocked launch is retried.** A docked craft loses its drive, sits, gets one fitted,
   and departs — with nothing re-notifying the autopilot. This is what replaced the
   `ready` flag.
6. **Loading obeys `atEndpoint()`.** Engaging while docked at the current endpoint loads the
   pods; engaging while docked somewhere off-route loads nothing.

**Play-test, still outstanding.** Dock, undock, ascend and descend on a shuttle and an IOS,
by mouse *and* keyboard, with and without an orbital present. An IOS should offer neither
ascend nor descend; a shuttle should never offer the drive. The Ascend button should appear
on the ground rather than in orbit, Dock and Undock should never draw together, and the
autopilot panel should stay engaged rather than switching itself off.
