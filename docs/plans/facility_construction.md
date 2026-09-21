# Pod work: construction on the clock

> Follows [craft_state.md](craft_state.md), which put the craft's activity behind
> `CS_WORKING` and its permissions behind `canX()` guards. Activating a pod was the one
> activity that never joined; most of it has since.

## Purpose

- **Deploying a section takes game time.** Activating a pod puts the craft into `CS_WORKING`;
  the effect lands when the timer expires, not on the click.
- **Interrupted work produces nothing.** The effect is applied at the end, so an abort simply
  never reaches it — only the cargo has to be settled.
- **A pod-load deploys as one run.** When a section finishes, the next pod carrying the same
  cargo starts by itself, so a craft with three frames needs one click.
- **The parameters are data, the effects are code.** Timings, consumption and chaining come
  from `item_work` in the database; what deploying a frame actually *does* stays a switch on
  item type, because expressing it as data would need an action vocabulary that still has
  code behind every entry.

## Delivered

**Work parameters are data.** `item_work` is a sparse linked table — a row exists only for
cargo that can be worked, so presence *is* the predicate and nothing reads a zero as absence.
Loaded beside `item_build_requirements` in
[`Loader::loadItems`](../../src/loaders/loader.cpp#L260) and written back in
[`SaveGame`](../../src/loaders/save_game.cpp#L648), so a save carries the definitions as it
already does for item build costs.

| item | work_time | consumption | abort_consumes | auto_continue |
|---|---|---|---|---|
| `Of_Frame` | 20 | 1 | 1 | 1 |
| `R_Frame` | 30 | 1 | 1 | 1 |
| `Bandaid` | 1 | 0 | 0 | 0 |

`Bandaid`'s duration is the one computed case: `activatePod` adds
`damage / BANDAID_REPAIR_RATE` to the tabulated base, so the table supplies the fixed
overhead and the code the variable part.

**Work has a subject.** `Craft::active_pod_index` names the pod driving the current
`CS_WORKING`, or `-1`. That matters because the autopilot uses the same state for loading
cargo — `-1` is what separates the two, and what the expired timer checks before applying
anything. Persisted on `craft`, so a craft saved mid-deployment resumes rather than reloading
as one working on nothing.

**One completion path.** Everything finishes through the `CS_WORKING` arm of the expiry
switch in [`Craft::update`](../../src/state/craft.cpp#L408): it calls `Game::onWorkComplete`,
consumes `work_parameters.consumption`, clears `active_pod_index`, and chains if
`auto_continue`. `updateActivePod` no longer ends work itself — a finished repair shortens the
timer to `0.001f` instead, so it comes back through the same arm. Only the active pod is
ticked; ticking all of them meant a Bandaid aboard repaired a facility nobody activated it on.

**Chaining stops by itself.** Nothing checks for completion: `canActivatePod` already refuses
once a facility is operational, so the chain ends and leftover cargo is kept. Covered by
*"construction chains across pods and stops when the facility is done"* in
[tests/test_craft_actions.cpp](../../tests/test_craft_actions.cpp).

## Remaining

### 1. Abort

`CA_CANCEL_WORK` is in the action enum, `abort_consumes` is in the table and loaded, and
[`Game::onWorkCancelled`](../../src/state/game.cpp#L1035) is an empty stub. What is missing is
the verb, its guard, and a control:

```cpp
CraftActionResult Game::canAbortWork(Craft *craft) const
{
    return craft->working() ? CAC_OK : CAC_WRONG_STATE;
}

void Game::onWorkCancelled(Craft *craft)
{
    if (craft->active_pod_index >= 0)
    {
        Pod &pod{craft->pods[craft->active_pod_index]};
        const Item &item{items[pod.contentType]};
        if (item.work_parameters.abort_consumes)
        {
            // A part-deployed frame is scrap, so abandoning is a real decision rather than
            // a free undo. A Bandaid keeps its charge: the repair so far is already banked.
            pod.amount = std::max(0, pod.amount - item.work_parameters.consumption);
        }
    }
    craft->setState(CS_IDLE);
    craft->active_pod_index = -1;
}
```

Because the effect only ever lands in `onWorkComplete`, there is no half-applied facility to
unwind. The motivating case is a craft attacked mid-build; nothing calls it automatically yet,
so the verb lands first and the caller follows.

### 2. Named section requirements

`8` and `2` are still literals in `onWorkComplete`, and `8` is repeated in the progress
readout at [shuttle_view.cpp:442](../../src/pages/shuttle_view.cpp#L442) as `* 100 / 8`. The
requirement is fixed by design, so name it:

```cpp
static const uint8_t SECTIONS_REQUIRED = 8;   // Orbital
static const uint8_t SECTIONS_REQUIRED = 2;   // ResourceFacility
```

### 3. Dock the shuttle in a surface station it just finished

Game logic rather than a notification, so it goes inline where the facility completes:

```cpp
if (++rf->construction_progress >= ResourceFacility::SECTIONS_REQUIRED)
{
    rf->operational = true;
    // A surface station is built around the craft, so it ends up inside it. An orbital is
    // not: the craft is floating in the orbit region, and closing with the station is a
    // manoeuvre it still has to fly -- so this is for surface facilities only.
    if (craft->type == CT_SHUTTLE) { craft->onDocked(); }
}
```

**No event change is needed for any of this.** The existing events already pass the facility,
so a sink reads `operational` and `construction_progress` straight off it — as
[shuttle_view.cpp:430](../../src/pages/shuttle_view.cpp#L430) does. Both raisers fire only
from `onWorkComplete`, and `canActivatePod` refuses once a facility is operational, so
`operational == true` on the event is a reliable one-shot "this section finished it" — which
is what faction response would key off.

### 4. Smaller

- **`canActivatePod` returns `bool`** while every other guard returns `CraftActionResult`, so
  the pod icon can only be hidden, not explained. It also has no busy check of its own — safe
  only because its callers happen not to try mid-work.
- The `// can work auto-continue? //TODO` comment above the chaining block is stale, and the
  `pod_idx != active_pod_index` test in it is dead: the index is cleared to `-1` two lines
  earlier. The chain works because consumption empties the pod instead. That only matters if
  a `pod_capacity` ever exceeds 1, when the just-worked pod *should* be reconsidered first.

## Verification

`make && make tests && ./bin/Debug/tests` — 65 cases / 7485 assertions is the current
baseline, and the run should stay free of `ERROR` lines beyond the six deliberate
negative-path ones.

Already covered: chaining across pods, stopping on completion with cargo kept, the first
section not landing on the activating tick, only the active pod ticking, `item_work`
round-tripping through a save, and a craft saved mid-deployment resuming and finishing its
section.

To add with the work above:

1. **Abort applies nothing and costs the frame.** Activate, abort mid-timer: progress
   unchanged, pod empty. A Bandaid abort keeps its charge and the damage already repaired.
2. **Auto-dock is surface-only.** Finishing a resource facility leaves the shuttle docked in
   it; finishing an orbital leaves the craft in the orbit region, undocked.
3. **Completion is observable once.** A test sink counting construction events seen with
   `facility->operational` true across a full build sees exactly one.


**Play-test:** build an orbital from an IOS carrying frames, watch the progress log advance
over time rather than jumping, abort part way and confirm the frame is lost, then finish a
surface facility and confirm the shuttle ends up docked inside it.

## Deferred

- **A per-item work descriptor in code.** `item_work` is that descriptor for the parameters;
  the effects remain a switch until there are enough of them to make a registry pay.
- **A work type distinct from the pod.** Needed only when one cargo can do more than one job;
  an asteroid mining attachment is the likely first case.
- **Multi-unit cargo.** `consumption` and the chain are written for it, but every activatable
  item has `pod_capacity` 1, so nothing exercises it.
- **Slow UI progress without a clock advance**, as the original had. The timed state supplies
  the data; whether the cockpit animates it is a presentation decision.
- **Bandaid reaches the ground from orbit.** `resourceFacilityAt` resolves through `body()`,
  and `canActivatePod`'s Bandaid case requires `docked()` but not which side. A rule question,
  not a bug to fix blind.
- **Two craft building the same facility.** Works by accident, since progress lives on the
  facility. Worth deciding deliberately before it is relied on.
