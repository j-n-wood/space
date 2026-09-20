# Pod work: construction on the clock

> Follows [craft_state.md](craft_state.md), which put the craft's activity behind
> `CS_WORKING` and its permissions behind `canX()` guards. Activating a pod is the one
> activity that never joined.

## Purpose

- **Deploying a section takes game time.** Activating a pod puts the craft into `CS_WORKING`;
  the effect lands when the timer expires, not on the click.
- **What a pod does is three questions about its cargo** — how long the work takes, what
  finishing it does, and what abandoning it costs. All three switch on the item type.
- **A pod-load deploys as one run.** When work finishes, the next pod carrying the same cargo
  starts automatically, so a craft with three frames needs one click.
- **Completion is visible to the game.** The construction event carries progress and whether
  this section completed the facility, so a sink can drive faction response — and a shuttle
  that finishes a surface station ends up docked inside it.

## Current behaviour

[`Game::activatePod`](../../src/state/game.cpp#L772) applies construction in full, inside the
click:

```cpp
case ItemType::Of_Frame:
{
    Orbital *orbital = orbitalAt(craft->location);
    if (!orbital) { orbital = createOrbital(craft->location); }
    if (++orbital->construction_progress >= 8) { orbital->operational = true; }
    raiseOrbitalConstructionEvent(orbital);
    pod.amount = 0;
}
```

- **No elapsed time.** The original game showed slow UI progress without advancing the clock;
  here there is neither.
- **The event cannot say what happened.** It fires identically for a section and for the one
  that completes the facility, so the only sink
  ([shuttle_view.cpp:430](../../src/pages/shuttle_view.cpp#L430)) reaches into
  `orbital->operational` to tell them apart, and repeats the literal `8` to compute a
  percentage.
- **`8` and `2` are literals.** The requirement is fixed by design for now, but it should be
  named.

`pod.amount = 0` is **correct**: `pod_capacity` is 1 for every activatable tool item
(verified — OF Frame, R Frame, Bandaid, Grapple and AMA are all 1; Derrick's 8 is not
construction cargo). A decrement is the generalisation if a capacity ever rises, not a fix.

`Bandaid` is the one item already on the clock: `activatePod` sets a timed `CS_WORKING` and
`updateActivePod` ticks the repair. It shows the shape, and also the thing to avoid — it ends
the work *itself* by setting `CS_IDLE` mid-tick, so there are two ways for work to finish.

## Design

### Work needs a subject

`CS_WORKING` says the craft is working; it cannot say what at, and the autopilot already uses
the same state for loading cargo. So the craft carries the pod driving the work:

```cpp
// craft.h
int8_t active_pod{-1};   // pod driving the current CS_WORKING, or -1 for none
```

`-1` separates autopilot loading from pod work, and it is what an expired timer looks up to
decide whether anything should happen. Persisted, so a craft saved mid-deployment resumes.

A pod index rather than a work-type enum: the cargo already names the activity. That holds
until one payload can do more than one thing — an asteroid mining attachment might — and a
work type can be added then, against a case that exists.

### Three switches on item type

What a pod does as work is defined by three functions, each switching on the cargo:

```cpp
// game.cpp
float Game::workDurationFor(const Craft *craft, const Pod &pod) const;
void  Game::completeWork(Craft *craft);   // timer expired: apply the effect
void  Game::abortWork(Craft *craft);      // abandoned: settle the cost
```

**Duration** is a sparse table with a default, since most items want a flat time and only a
few compute one:

```cpp
namespace
{
    struct PodWorkTime { ItemType item; float seconds; };

    // Sparse on purpose: anything not listed gets the default. Distinct from
    // Item::production_time, which is how long a factory takes to BUILD the item.
    const PodWorkTime podWorkTimes[] = {
        {ItemType::Of_Frame, 20.0f},
        {ItemType::R_Frame, 30.0f},
    };
    const float DEFAULT_POD_WORK_TIME = 5.0f;
}

float Game::workDurationFor(const Craft *craft, const Pod &pod) const
{
    const ItemType item = static_cast<ItemType>(pod.contentType);
    if (item == ItemType::Bandaid)
    {
        // computed, not tabulated: a worse-damaged station takes longer
        ResourceFacility *rf = resourceFacilityAt(craft->location);
        return 1.0f + (rf ? rf->damage / BANDAID_REPAIR_RATE : 0.0f);
    }
    for (const PodWorkTime &w : podWorkTimes)
    {
        if (w.item == item) { return w.seconds; }
    }
    return DEFAULT_POD_WORK_TIME;
}
```

**Completion** applies the effect and decides what happens next:

```cpp
void Game::completeWork(Craft *craft)
{
    if (craft->active_pod < 0) { return; }   // autopilot loading -- nothing to apply

    Pod &pod = craft->pods[craft->active_pod];
    const ItemType item = static_cast<ItemType>(pod.contentType);

    switch (item)
    {
    case ItemType::Of_Frame:
    case ItemType::R_Frame:
    {
        Facility *f = deploySection(craft, item);   // creates the facility if needed
        pod.amount = 0;                             // capacity is 1: the frame is spent
        if (f && f->operational)
        {
            onFacilityComplete(craft, f);
            break;                                  // finished: do not chain
        }
        // Activate all: carry on with the next pod of the same cargo, so a pod-load is
        // one run rather than one click per section. Default because there is no UI for
        // choosing otherwise, and it is the common case.
        activateNextMatching(craft, item);
        break;
    }
    case ItemType::Bandaid:
        // updateActivePod has been reducing damage all along; expiry just ends it
        break;
    default:
        break;
    }

    if (!craft->working()) { craft->active_pod = -1; }
}
```

**Abort** settles the cost, which is also per item:

```cpp
void Game::abortWork(Craft *craft)
{
    if (craft->active_pod >= 0)
    {
        Pod &pod = craft->pods[craft->active_pod];
        switch (static_cast<ItemType>(pod.contentType))
        {
        case ItemType::Of_Frame:
        case ItemType::R_Frame:
            // A part-deployed frame is scrap. Abandoning construction loses it, so an
            // abort is a real decision rather than a free undo.
            pod.amount = 0;
            break;
        case ItemType::Bandaid:
            // Repair applies continuously, so the work done is already banked and the
            // remaining charge is still usable.
            break;
        default:
            break;
        }
    }
    craft->setState(CS_IDLE);
    craft->active_pod = -1;
}
```

Because the effect is applied only in `completeWork`, an abort cannot leave a facility half
changed — the only thing to settle is the cargo.

### The cycle

```cpp
// activation starts work; it no longer changes the world
CraftActionResult Game::activatePod(Craft *craft, int pod_index)
{
    CraftActionResult can = canActivatePod(craft, pod_index);
    if (!can) { return can; }

    craft->active_pod = static_cast<int8_t>(pod_index);
    craft->work(workDurationFor(craft, craft->pods[pod_index]));
    return CAC_OK;
}
```

Completion moves into the `CS_WORKING` arm of the expiry switch, which currently carries the
TODO this plan closes:

```cpp
// craft.cpp, Craft::update expiry switch
case CS_WORKING:
    Game::getCurrent()->completeWork(this);
    break;
```

### The event says what happened

The sink already has to distinguish a section from a completion, and currently does it by
reading `orbital->operational` and dividing by a literal `8`. Put both in the payload:

```cpp
// event_sink.h -- one signature for both facility kinds; the sink can check type
virtual void onFacilityConstruction(Facility *facility, uint8_t progress, uint8_t required,
                                    bool complete);
```

`progress` and `required` give the percentage without the sink knowing the requirement;
`complete` is the hook faction response hangs off. The two existing raise functions collapse
into one.

```cpp
void Game::onFacilityComplete(Craft *craft, Facility *facility)
{
    // A surface station is built around the craft, so it ends up inside it. An orbital is
    // not: the craft is floating in the orbit region, and closing with the station is a
    // manoeuvre it still has to fly.
    if (!facility->inOrbit() && craft->type == CT_SHUTTLE)
    {
        craft->onDocked();
    }
}
```

### Guards

`canActivatePod` returns a reason and gains the busy check it lacks today:

```cpp
CraftActionResult Game::canActivatePod(Craft *craft, int pod_index) const
{
    if (craft->moving() || craft->working()) { return CAC_BUSY; }
    // ... existing per-item position and facility checks, returning CAC_WRONG_STATE
}
```

That stops a click landing while the autopilot is loading, and lets the pod icon show why it
is inert. `CA_CANCEL_WORK` is already in the action enum and unused — `canAbortWork(craft)`
is `working() ? CAC_OK : CAC_WRONG_STATE`.

### Constants

The requirement is fixed by design, so name it rather than repeating it:

```cpp
static const uint8_t SECTIONS_REQUIRED = 8;   // Orbital
static const uint8_t SECTIONS_REQUIRED = 2;   // ResourceFacility
```

## Data

- `craft.active_pod INT`, defaulting to `-1`. Saves are still disposable, so this is free now.
- No facility schema change: `construction_progress` and `operational` already persist.

## Files

| File | Change |
|---|---|
| [include/state/craft.h](../../include/state/craft.h) / [craft.cpp](../../src/state/craft.cpp) | `active_pod`; the `CS_WORKING` expiry arm calls `completeWork` |
| [include/state/game.h](../../include/state/game.h) / [game.cpp](../../src/state/game.cpp) | `activatePod` starts work only; `workDurationFor`, `completeWork`, `abortWork`, `deploySection`, `activateNextMatching`, `onFacilityComplete`; `canActivatePod` / `canAbortWork` return results |
| [include/state/event_sink.h](../../include/state/event_sink.h) | `onFacilityConstruction` replaces the two per-kind events |
| [include/state/orbital.h](../../include/state/orbital.h), [resourceFacility.h](../../include/state/resourceFacility.h) | `SECTIONS_REQUIRED` |
| [src/pages/shuttle_view.cpp](../../src/pages/shuttle_view.cpp) | pod icon shows the refusal reason; log uses the event payload; abort control |
| [src/loaders/](../../src/loaders/) | `active_pod` column |
| `tests/test_construction.cpp` | new — **needs `./reconf.sh`** |

## Steps

Build and test after each.

1. **`active_pod` plus persistence.** Set and cleared, read by nothing yet. Additive.
2. **`canActivatePod` returns `CraftActionResult`** and gains the busy check; the UI shows the
   reason. No timing change yet.
3. **Move construction onto the clock.** `workDurationFor`, `completeWork`, the expiry arm.
   Behavioural, and the step that needs the play-test.
4. **`abortWork`** with its per-item cost, `canAbortWork`, and a UI control.
5. **Sequencing.** `activateNextMatching`, driven from `completeWork` by item type.
6. **Event payload and constants.** `onFacilityConstruction` with progress and completion;
   `SECTIONS_REQUIRED`; `onFacilityComplete` and the surface auto-dock.
7. **Unify `Bandaid`** so `updateActivePod` only applies continuous effect and the timer is
   the single way work ends.

## Verification

`make && make tests && ./bin/Debug/tests` — 61 cases / 7411 assertions is the current
baseline.

1. **Construction takes time.** Activating a frame pod leaves `construction_progress`
   unchanged on the same tick, and raises it only once the timer expires.
2. **Abort applies nothing and costs the frame.** Activate, abort mid-timer: progress
   unchanged, pod empty. A Bandaid abort keeps its charge and the damage already repaired.
3. **The chain runs and stops.** Three pods of one frame each deploy three sections from one
   activation; the chain stops when the facility completes, even with cargo left.
4. **Completion is reported once.** A test sink counts `onFacilityConstruction` calls with
   `complete == true` across a full build: exactly one, on the section that sets
   `operational`, and `progress == required` on that call.
5. **Auto-dock is surface-only.** Finishing a resource facility leaves the shuttle docked in
   it; finishing an orbital leaves the craft in the orbit region, undocked.
6. **Resume across save/load.** A craft saved mid-deployment reloads working, with
   `active_pod` intact, and completes the section.
7. **Loading is not pod work.** An autopilot craft in `CS_WORKING` with `active_pod == -1`
   completes its load without touching any facility.
8. **Busy refuses.** `canActivatePod` returns `CAC_BUSY` while working or manoeuvring — one
   of the few direct `CAC_BUSY` assertions anywhere, which `craft_state.md` notes is missing.

**Play-test** after step 3 and again after step 6: build an orbital from an IOS carrying
frames, watch the progress log advance over time, abort part way and confirm the frame is
lost, then finish a surface facility and confirm the shuttle ends up docked inside it.

## Deferred

- **A per-item work descriptor.** Three switches on item type is the right size for three
  activities. When there are more — mining, grapple, AMA — they want to be one table of
  `{duration, onComplete, onAbort, chains}` rather than three parallel switches.
- **A work type distinct from the pod.** Needed only when one cargo can do more than one
  job; an asteroid mining attachment is the likely first case.
- **Slow UI progress without a clock advance**, as the original had. The timed state supplies
  the data; whether the cockpit animates it is a presentation decision.
- **Abort under attack.** The motivating case for `abortWork`, but nothing calls it
  automatically yet — the verb lands first, the caller follows.
- **Two craft building the same facility.** Works by accident today, since progress lives on
  the facility. Worth deciding deliberately before it is relied on.
