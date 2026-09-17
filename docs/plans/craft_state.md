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

## What needs to be done

1. **Add the `canX()` methods** and move each verb's inline guards into them, in tier order.
   Verbs become `if (!canX()) return *this;` plus the effect. No UI change yet, so this step
   is behaviour-preserving and testable on its own.

2. **`Game::craftCanDock` → `checkCraftCanDock`** returning `CraftActionResult`
   (`CAC_NO_ORBITAL` / `CAC_ORBITAL_INCOMPLETE` / `CAC_DEFENDED`), called from `canDock()` as
   its situation tier. Fixes the already-docked case by construction, since `canDock()` also
   requires the bare orbit region.

3. **Point `ShuttleView` at the `canX()` methods** — twelve call sites across `render()` and
   `input()` ([shuttle_view.cpp:153-209](../../src/pages/shuttle_view.cpp#L153) and
   [:383-401](../../src/pages/shuttle_view.cpp#L383)). Mouse and keyboard currently ask
   differently; both become the same call. This is where the three bugs above disappear.
   Consider a control table (`{action, rect, tooltip}`) so the two paths cannot drift again.

4. **`engageAutopilot` returns `CraftActionResult`** rather than `bool`, so
   [autopilot_view.cpp:48](../../src/pages/autopilot_view.cpp#L48) can report why engaging
   failed. `CAC_NO_SUPPLY_POD` exists for this.

5. **Autopilot asks the same questions.** [autopilot.cpp](../../src/state/autopilot.cpp)
   repeats capability checks the verbs now make; replace with `canX()` so a refused leg can
   disable the autopilot with a logged reason instead of retrying.

### Smaller items, still open

- **`dock()` writes `state` and `state_timer` raw**, leaving `total_state_timer` stale, so
  `stateProgress()` misreads for the whole approach. The other verbs use `setTimedState`.
- **`onSpacecraftDocked` is gated twice**: `CC_INTERPLANETARY` at
  [craft.cpp:260](../../src/state/craft.cpp#L260) and `CC_BOARDING` inside
  [game.cpp:1019](../../src/state/game.cpp#L1019). Identical today, different in meaning —
  drop the outer one.
- **`Craft::update` gates the autopilot on `drive`.** With fitment checked at engagement, the
  gate becomes `autopilot->state >= AS_ON`.
- **`Craft::update` is still `virtual`** with no overrides left.
- **`createShuttle` leaves `name` empty**, so autopilot log lines read "Autopilot:  ...".
- **`architecture.md:90-105`** still lists the pre-collapse enum.

---

## Verification

`make && make tests && ./bin/Debug/tests` — 52 cases / 7319 assertions is the current
baseline. Add to `tests/test_craft_state.cpp` (**needs `./reconf.sh`** — premake globs at
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

**Play-test** after step 3: dock, undock, ascend and descend on a shuttle and an IOS, by
mouse *and* keyboard, with and without an orbital present. An IOS should offer neither ascend
nor descend; a shuttle should never offer the drive. Confirm the Ascend button now appears on
the ground rather than in orbit.
