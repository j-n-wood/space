# Events, Page Lifecycle, and Simulation Testing

A design proposal covering a typed event bus, the page lifecycle hooks it depends on, and the testing strategy it enables.

## Context

The game state is a deterministic simulation driven by `Game::update(float delta)` ([src/state/game.cpp:559](../src/state/game.cpp#L559)). Many systems progress over simulated time and involve multi-step sequences:

- **Autopilot resource transfer** — ascend → orbit-dock → resource cycling → launch → descend → surface-dock → unload → repeat ([src/state/autopilot.cpp](../src/state/autopilot.cpp)).
- **Orbital construction** — 8 sequential `Of_Frame` pod activations gated by autopilot docking ([src/state/game.cpp:502-527](../src/state/game.cpp#L502-L527)).
- **Factory production** — tick-based, advances per integer crossing of `game_time` ([src/state/game.cpp:606-612](../src/state/game.cpp#L606-L612)).
- **Research and craft state machines** — continuous deltas with countdown timers.

The current test suite ([tests/test_db.cpp](../tests/test_db.cpp)) only covers persistence round-trips. **Nothing exercises `update()` over time**, so regressions in any time-driven behaviour (autopilot stalls, factory off-by-one, orbital never completing) go uncaught.

Two hazards drive this design:

1. **Frame-time variability** — Raylib's `GetFrameTime()` is not deterministic; tests must control `delta`.
2. **Constant drift** — `CSTD_ASCENT`, item `build_time`, transit speed, and the literal `8` for orbital pods can all change. Tests hardcoding "after 4 seconds" or "after 8 steps" become liabilities.

### Key architectural insight: one event system, multiple consumers

The clean simulation/UI separation means **the test observability problem and the UI reactivity problem are the same problem**. `Game::update` runs blind to whether a factory view is open or a test is running; both consumers want to *react* to meaningful state changes (production complete, craft docked, construction progressed) rather than poll state every frame.

The codebase already has a primitive of this shape: [include/state/log_sink.h](../include/state/log_sink.h) defines a `LogSink` interface, `NullLogSink` is the default, and [PageLog](../include/pages/page_log.h) consumes formatted-string events from `Game::activatePod` ([src/state/game.cpp:485-545](../src/state/game.cpp#L485-L545)). This works for human-readable scroll-back text but is unergonomic for:

- **Structured UI updates** — `FactoryView` wants `{factory, item, progress, required}`, not a string to re-parse.
- **Test assertions** — tests want to filter and count typed events, not regex strings.
- **Overview/alert pages** — want "anything just completed?" across all facilities, not their own polling logic.

The proposal: **extend `LogSink` from a string sink into a typed-event publish/subscribe system** that all three consumers — PageLog text formatting, reactive UI views, test recorders — sit on top of.

---

## Strategy

### Principle 1 — Drive simulation with a fixed, small delta

Tests never use real frame time. A test-only helper steps `Game::update(delta)` with a controlled small fixed step (`1/60s` is a good default).

- `runFor(game, seconds)` — advance exactly that much sim-time. Use when a precise duration window matters.
- `runUntil(game, predicate, maxSeconds)` — step until predicate true, fail past `maxSeconds`. Use when the test cares about *reaching a state*.

Step size must be `<< 1.0s` so the tick-aligned branch in `Game::update` ([src/state/game.cpp:606-612](../src/state/game.cpp#L606-L612)) is not skipped, and should not divide 1.0 evenly (avoid landing exactly on tick boundaries every iteration). `1/60` satisfies both. `MAX_TIMESTEP = 1.0f` ([src/state/game.cpp:12](../src/state/game.cpp#L12)) is the upper clamp.

### Principle 2 — Replace string-based `LogSink` with a typed event bus

Generalise the existing `LogSink` concept. Instead of `addLog(const char*)`, the simulation publishes **typed event values** to a bus, and subscribers consume the ones they care about. The design follows the codebase's existing idioms — no `std::function`, no `std::unordered_map`, no `std::string`. Only `std::vector` and virtual dispatch, both of which are in heavy use already.

**Shape of the system:**

- **`Event` family** — a closed set of small, copy-cheap plain structs, one per meaningful occurrence. Each carries the payload its consumers actually need (entity pointers, enums, counts, item ids). Pointers are stable for entity lifetimes in this single-process simulation, so passing `Factory*`, `Craft*`, `Orbital*` directly is fine.

  Initial set, derived from the time-driven systems:
  - `OrbitalConstructionStarted { Location* }`
  - `OrbitalConstructionProgressed { Orbital*, int progress, int required }`
  - `OrbitalOperational { Orbital* }`
  - `ResourceFacilityConstructionProgressed { ResourceFacility*, int, int }`
  - `ResourceFacilityOperational { ResourceFacility* }`
  - `CraftStateChanged { Craft*, CraftState from, CraftState to }`
  - `AutopilotDocked { Craft*, Facility* }`
  - `AutopilotLaunched { Craft*, Facility* }`
  - `ResourceTransferred { Stores* from, Stores* to, ResourceType, int amount }`
  - `FactoryProductionStarted { Factory*, ItemType }`
  - `FactoryProductionProgressed { Factory*, int progress, int required }`
  - `FactoryProductionCompleted { Factory*, ItemType }`
  - `ResearchProgressed { ResearchFacility*, Topic*, float fraction }`
  - `ResearchCompleted { ResearchFacility*, Topic* }`

  Concrete typed structs — not a generic `{string type, map<string,any> payload}` — because generic payloads are unergonomic for UI consumers and would force allocation/type-erasure machinery the project avoids.

- **`EventSubscriber` interface** — an abstract class with one `virtual void on...(const X&)` method per event family. The base implementation of every method is a no-op (the methods are virtual, not pure virtual). Subscribers override only the methods they care about. Example skeleton: `virtual void onOrbitalConstructionProgressed(const OrbitalConstructionProgressed&) {}`.

  This replaces both the heterogeneous `std::function` storage and any `std::type_index` keying. Type safety is enforced by the method signatures; dispatch is a single virtual call per subscriber per event. The recording subscriber used by tests overrides every method and pushes the event into a vector.

- **`EventBus`** — owned by `Game`, accessible via `game.events()`. Holds a single `std::vector<EventSubscriber*>` of registered subscribers (non-owning pointers; subscribers manage their own lifetime). `subscribe(EventSubscriber*)` appends, `unsubscribe(EventSubscriber*)` removes by pointer equality. Bounded subscriber count in practice (PageLog formatter, optionally one or two visible UI views, optionally a recorder under tests — typically 1-3 active).

- **Per-event publish methods on the bus** — one typed method per event family: `bus.publishOrbitalConstructionProgressed(const OrbitalConstructionProgressed&)`. Implementation is a small loop: `for (auto* sub : subscribers) sub->onOrbitalConstructionProgressed(event);`. No map lookup, no type tag, no type erasure. Production code calls e.g. `game.events().publishOrbitalConstructionProgressed({orbital, progress, required});` at the relevant site. With zero subscribers in non-UI/non-test runs the loop body is empty — effectively free.

- **Single global bus, filter in handler.** Per-entity subjects are not introduced. UI subscribers that care about one entity check `event.factory == myFactory` inside their `on...` override. Cheap and keeps wiring centralised.

- **Synchronous, single-threaded delivery.** Publish on the simulation thread calls subscribers immediately. No queue. Document "do not publish from inside a handler" as a hazard; optionally a debug-time re-entrancy assert.

- **Cost of the closed event set.** Adding a new event family touches three places: the struct definition, an `on...` method on `EventSubscriber` (with default empty body), and a `publish...` method on `EventBus`. Mechanical, no fan-out across subscribers — existing subscribers continue to compile unchanged because the new virtual is a no-op by default. Trade-off accepted in exchange for not needing a map.

### Principle 3 — Three consumer roles sit on the same bus

**(a) Text formatter for `PageLog`.**
`TextFormatter` is a concrete `EventSubscriber` that overrides every `on...` method and converts the event payload to a one-line human string via `snprintf`, then forwards into `PageLog::addLog`. This moves the `snprintf` blocks out of `activatePod` and into a single formatting module — same in-game scroll-back the user sees today, with a cleaner separation. `LogSink` itself can shrink to a thin string-line interface that `PageLog` keeps for its render path; the bus is the new producer side.

**(b) Reactive UI views.**
`FactoryView` (and analogous pages) is an `EventSubscriber`. It registers itself on the bus during `activate()` and **unregisters during `deactivate()`** (see Principle 4 — that lifecycle hook does not yet exist and must be introduced first). The view overrides `onFactoryProductionProgressed` (and similar), filters `event.factory == this->factory` inside the override, and updates a member used by `render()`. The view no longer polls factory state every frame for "is something interesting about to happen" — it reacts. Overview / alerts pages override completion methods globally and surface them as alerts.

**(c) Test recording subscriber.**
`RecordingSubscriber` is an `EventSubscriber` in test code that overrides every `on...` method and pushes a tagged copy of the event (with `game_time`) into an internal `std::vector` of small variant-like records (or one vector per event family, since the closed set is small). Tests query the recording:

- `recording.countOf<OrbitalConstructionProgressed>() == 7`
- `recording.lastOf<OrbitalOperational>().orbital == luna_orbital`
- `recording.between(t0, t1).countOf<CraftStateChanged>()` for ordering assertions.

If the variant-style record turns out to be awkward in this style, the simpler shape is one `std::vector` per event family on the recorder — explicit, no templates, easy to scan. Tests pick whichever vector matches the assertion. This is robust to simulation speed and step size — only the *sequence* and *count* of meaningful transitions is asserted, not exact frame counts.

### Principle 4 — Page lifecycle is owned by PageManager, with reliable deactivate

UI subscribers must never outlive their page's active period. The current code does not support this safely:

- `BasePage` defines `activate(viewState)` ([include/pages/base_page.h:54](../include/pages/base_page.h#L54)) but **no `deactivate`**.
- `PageManager::switchToPage` ([src/pages/pages.cpp:47-60](../src/pages/pages.cpp#L47-L60)) calls `activate` on the new page and does nothing to the outgoing page.
- Quickload (F8) calls `currentPage->activate(...)` directly to force a refresh ([src/main.cpp:236](../src/main.cpp#L236), with a `// TODO ugly` already flagging this) — meaning the page transitions from active → active without an intervening teardown.

Adding subscriptions on top of this would leak callbacks across page switches and double-subscribe on quickload. The fix is small and stands on its own merit (the `TODO ugly` goes away):

1. **Add `virtual void BasePage::deactivate()`** with a default no-op implementation. Pages that hold subscriptions or other resources override it.
2. **`PageManager::switchToPage` calls `deactivate` on the outgoing page** before `activate` on the incoming page. The block at [src/pages/pages.cpp:54-58](../src/pages/pages.cpp#L54-L58) becomes: if changing, call `currentPage->deactivate()` first, then assign and call `newPageInstance->activate(viewState)`.
3. **Add `PageManager::reactivateCurrent()`** — calls `deactivate` then `activate` on the same page, idempotent and safe. Quickload at [src/main.cpp:236](../src/main.cpp#L236) switches to using this method instead of calling `activate` directly; the `TODO ugly` is resolved.
4. **The invariant becomes:** a page is *active* iff `PageManager.currentPage` points at it, and every active → inactive transition (whether to another page or via reactivate) passes through `deactivate` first. PageManager is the single owner of these transitions; pages never call activate/deactivate on themselves.

**Subscription storage in pages:** views hold their subscription handles as RAII members. The handle's destructor removes the callback from the bus. `deactivate` empties the handle container (or assigns fresh empties). A defensive precondition in `activate` — "if I already hold handles, that's a bug" — can catch any future regression of the lifecycle invariant via assert in debug builds.

With this in place, page subscriptions are safe in both the normal switch path and the save/load reactivation path. No idempotency hacks are needed inside subscription logic; the lifecycle hook is the right place to enforce it.

### Worked example — `FactoryView` reacts to production completion

`FactoryView` ([include/pages/factory_view.h](../include/pages/factory_view.h), [src/pages/factory_view.cpp:12](../src/pages/factory_view.cpp#L12)) currently does no reactive work — it resolves its target `Factory*` in `activate()` and reads state during `render()`. To make it flash a "completed" indicator the moment production finishes, the change is:

**Header sketch:**

```cpp
class FactoryView : public BasePage, public EventSubscriber
{
    Factory *factory;
    SublocationType sublocationType;
    float completionFlashTimer; // > 0 while we show the flash

public:
    FactoryView(SublocationType slt);
    ~FactoryView() {}

    void activate(ViewState &viewState) override;
    void deactivate() override;                            // new lifecycle hook
    void onFactoryProductionCompleted(                     // event override
        const FactoryProductionCompleted &e) override;
    void input() override;
    void render() override;
};
```

**Implementation sketch:**

```cpp
void FactoryView::activate(ViewState &viewState)
{
    factory = nullptr;
    completionFlashTimer = 0.0f;
    // ... existing factory resolution from activate() body ...

    Game::getCurrent()->events().subscribe(this);
}

void FactoryView::deactivate()
{
    Game::getCurrent()->events().unsubscribe(this);
}

void FactoryView::onFactoryProductionCompleted(
    const FactoryProductionCompleted &e)
{
    if (e.factory == factory)        // filter: only our factory
        completionFlashTimer = 1.5f; // start a 1.5s flash overlay
}

void FactoryView::render()
{
    // ... existing render ...
    if (completionFlashTimer > 0.0f) {
        // draw "Production complete!" badge with alpha derived from timer
        completionFlashTimer -= GetFrameTime();
    }
}
```

What this illustrates:

- `EventSubscriber` is mixed in via plain multiple inheritance. It has no state, only virtual methods with default-empty bodies, so there's no diamond hazard.
- The view only overrides the one method it cares about. `onCraftStateChanged`, `onResearchProgressed`, and the rest stay as the base no-ops.
- `event.factory == factory` is the filter — handler-side, single pointer compare, no per-entity subject machinery.
- `activate` and `deactivate` are the only places where bus state changes; PageManager (Principle 4) guarantees they are called in pairs and exactly once per active period.
- Save/load via `reactivateCurrent()` runs `deactivate()` (unsubscribing) then `activate()` (re-subscribing). No double-subscribe.
- Pure-render state (`completionFlashTimer`) lives on the view, not the simulation. The simulation publishes facts; the view chooses how long to remember them and how to display them.

The same shape applies to an overview/alerts page: subscribe in `activate()`, override every completion-type method, push items into an alert ring buffer; unsubscribe in `deactivate()`.

### Principle 5 — Assert on observable state and events, not internal timers

Prefer:
- Stores quantities, facility `operational` flags, factory output counts, event-trace shape.

Avoid:
- `CHECK_EQ(craft.state_timer, 2.0f)` — couples the test to internal countdown bookkeeping.
- `runFor(g, 4.0f); CHECK(craft.state == CS_ORBIT)` — couples to the magnitude of `CSTD_ASCENT`.

When timing genuinely matters, the bound references the live constant: `CHECK_LE(game_time_at_orbit, CSTD_ASCENT + CSTD_LAUNCH + epsilon)`. If `CSTD_ASCENT` changes 4→6, the assertion tracks it. The test encodes the *invariant* (ascent ≈ one ascent-duration), not the number.

For factory tests, the assertion uses the item's loaded `build_time` from the database rather than a literal. The orbital-construction literal `8` ([src/state/game.cpp:512](../src/state/game.cpp#L512)) becomes a named constant such as `ORBITAL_CONSTRUCTION_PODS`, similarly the `2` at line 537 and the `20.0f` speed factor in `TransitTimeCalculator`. Tests reference the names.

### Principle 6 — Predicate advancement is the default

Most behavioural tests care that *eventually* a state is reached and the right state-diff occurred, not the exact duration. `runUntil` makes that the natural shape:

> Run until `orbital->operational`; assert `recording.countOf<OrbitalConstructionProgressed>() == ORBITAL_CONSTRUCTION_PODS - 1` and `source_stores.get(Of_Frame_resource) == initial - ORBITAL_CONSTRUCTION_PODS * pod_capacity`.

The duration the test takes is incidental.

### Principle 7 — Composable scenario builders

Tests should not hand-roll multi-step setup. A small helpers module provides:

- `buildShuttleWithCargo(stores)` — Shuttle in a known state, supply pods loaded.
- `setupAutopilotFlow(craft, source, dest, flowFlags)` — wires autopilot endpoints.
- `seedFacilityWithItems(facility, ItemType, count)` — bypasses factory production for tests not about production.

Setup stays declarative; the test body shows only the system under test.

### Principle 8 — Tests run headless and deterministic

The tests target ([premake5.lua:287-315](../premake5.lua#L287-L315)) already links all state code and Raylib (for raygui symbols) but never opens a window. New behavioural tests preserve this — no PageManager, no Raylib calls, no `GetFrameTime`. Combined with fixed-step driving, every test is fully deterministic.

---

## What changes in production code

1. **Introduce `EventBus` and the Event struct family** in `include/state/events.h` (or a small folder if it grows). `EventBus` exposes `subscribe`, `unsubscribe`, and one `publish...` method per event family.

2. **Add `EventBus *Game::events()`** and a default-constructed bus owned by Game. Default has zero subscribers — every `publish...` is a no-op loop, costing essentially nothing.

3. **Publish events at meaningful state transitions.** Sites to instrument:
   - `Game::activatePod` ([src/state/game.cpp:485](../src/state/game.cpp#L485)) — orbital/resource-facility started, progressed, operational.
   - Craft state assignment in [src/state/shuttle.cpp](../src/state/shuttle.cpp), [src/state/ios.cpp](../src/state/ios.cpp), [src/state/craft.cpp](../src/state/craft.cpp).
   - `Autopilot::onDocked` / `onDockWorkComplete` / transit engage in [src/state/autopilot.cpp](../src/state/autopilot.cpp).
   - `Factory::update` per-tick progress and item completion.
   - `ResearchFacility::update` progress and topic completion.

4. **Replace inline `snprintf → LogSink` calls in `activatePod` with `publish...` calls.** Move the string formatting into a `TextFormatter` subscriber that converts each event to its current human-readable form and pushes into PageLog. Net code change in `activatePod` is a slight shrink and a clearer body.

5. **Wire `PageLog` (or its successor) up as a `TextFormatter` consumer** during game init where the current `LogSink*` is passed today. The existing `LogSink` interface can remain as the string-rendering target inside the formatter; what changes is *who produces strings* (formatter, not state code).

6. **Establish reliable page lifecycle (prerequisite for any UI subscription):**
   - Add `virtual void BasePage::deactivate()` (default no-op).
   - Modify `PageManager::switchToPage` ([src/pages/pages.cpp:47-60](../src/pages/pages.cpp#L47-L60)) to call `deactivate()` on the outgoing page before activating the new one.
   - Add `PageManager::reactivateCurrent()` for the save/load refresh path. Update [src/main.cpp:236](../src/main.cpp#L236) to use it (resolving the `// TODO ugly`).
   - Invariant: PageManager is the only caller of `activate`/`deactivate`. Page code never calls these on itself.

7. **`FactoryView` (and similar pages) subscribe to relevant event types in `activate()`** and unsubscribe in `deactivate()` (see worked example above). Renders consume the freshest event-derived state instead of recomputing every frame. **This depends on item 6 being in place first.**

8. **Introduce named constants** for the magic `8` (orbital pods), `2` (resource-facility pods), and `20.0f` (transit speed factor) so tests can reference them in assertion bounds.

No clock injection. No constant injection beyond naming the literals. No mocking of Raylib. `update(delta)` remains the testing seam.

---

## What does not change

- **No generic `{string type, blob payload}` events.** Generic payloads are unergonomic for UI consumers; typed events are mandatory.
- **No `std::function`, `std::unordered_map`, `std::map`, or `std::string`** anywhere in the bus — none are used elsewhere in the codebase and the bus shouldn't introduce them. Virtual dispatch + `std::vector<EventSubscriber*>` carries the whole design.
- **No fake clock.** `delta` parameter is already the seam.
- **No per-entity subjects.** A single bus + handler-side filtering matches codebase scale and idiom.
- **Existing persistence tests in [tests/test_db.cpp](../tests/test_db.cpp)** stay as-is — they cover save/load and don't need behavioural coverage.
- **`LogSink` interface stays alive** as PageLog's string-rendering input. It just isn't the producer-side API anymore.

---

## Test file organisation

- `tests/test_simulation.cpp` (new) — behavioural tests using fixed-step driver + recording subscriber.
- `tests/sim_test_support.{h,cpp}` (new) — `runFor`, `runUntil`, `RecordingSubscriber`, scenario builders.
- `tests/test_db.cpp` (existing) — unchanged, continues to own persistence coverage.

Initial coverage to target, in priority order:

1. **Autopilot loaded-flow round trip** — shuttle picks up at source, delivers at destination, returns. Assertions: stores delta matches expected transfers, recorded `AutopilotDocked` count, `ResourceTransferred` event sequence.
2. **Orbital construction completes after `ORBITAL_CONSTRUCTION_PODS` deliveries** — run autopilot loop until `orbital->operational`. Assert progressed event count and presence of operational event.
3. **Factory produces an item over `build_time` ticks** — run for `build_time` + epsilon, assert exactly one `FactoryProductionCompleted` and stores contains item.
4. **Research progresses with continuous time** — run for `topic.requiredTime`, assert completion event.
5. **Time-rate scaling** — same scenario at `time_rate=2.0` runs in half sim-time; event *sequence* identical, *timestamps* halved.

Each test: scenario setup → attach recording subscriber → `runUntil` predicate → assert on observable state + recording.

---

## Migration sequence

The change is large in surface area but each step is small and self-contained:

1. Add `EventBus` + a couple of `Event` types (e.g. the orbital-construction trio). No subscribers yet. `publish` is a no-op cost in production.
2. Add `TextFormatter` that converts those events to strings and forwards to PageLog.
3. Convert `activatePod` from inline `snprintf → LogSink` to `publish...` calls. Verify in-game scroll-back text is unchanged.
4. Add `RecordingSubscriber` in test support and write the first behavioural test (orbital construction). Now the system is validated end-to-end **without yet touching any UI subscribers**.
5. **Page lifecycle prerequisite:** introduce `BasePage::deactivate()`, fix `PageManager::switchToPage` to call it, add `reactivateCurrent()`, switch quickload over. Ship and verify normal page transitions and quickload still work the same.
6. Expand event coverage to factory, craft, autopilot, research one system at a time, adding behavioural tests as each is wired up.
7. **Only now** add UI-side subscriptions in `FactoryView` etc., relying on the stable lifecycle from step 5.

Each step leaves the project shippable. Steps 1-4 require zero UI changes. Step 5 stands alone as a small lifecycle refactor. Steps 6-7 are independent per system and can be reordered.

---

## Risks and trade-offs

- **Emit-site discipline.** Future state transitions could forget to publish events. Mitigation: brief CLAUDE.md note ("when adding a craft state or factory branch, publish an event near the state assignment"). Emit calls cluster near the transitions so they read as part of the change.
- **Closed event set requires changes in three places.** Adding a new event family means a new struct, a new `on...` method on `EventSubscriber` (default no-op), and a new `publish...` on `EventBus`. Existing subscribers compile unchanged. This is intentional — it trades a small mechanical overhead per new event family for avoiding `std::unordered_map<type_index, ...>` and `std::function`, which the codebase otherwise avoids.
- **Subscriber lifetime (highest-risk item).** UI subscribers must unsubscribe when their owning page deactivates, or the bus holds dangling callbacks that fire on stale `this` pointers — a class of bug that produces "strange behaviour" rather than clean crashes. Mitigations stacked together: (a) PageManager is the single owner of activate/deactivate transitions, so a page cannot accidentally be re-activated without prior deactivation; (b) `reactivateCurrent()` handles the quickload case explicitly; (c) `activate()` can assert "no leftover subscription state" in debug builds to catch lifecycle regressions early.
- **Re-entrant publishes.** A handler that publishes another event mid-callback is risky if it mutates state the outer iteration depends on. Mitigation: documented hazard, optional debug assert. Most subscribers will be read-only (UI render-state update, test recording).
- **Step-size choice.** `1/60s × 100 sim-seconds ≈ 6000 update calls per test`. With no rendering this stays in milliseconds; not a concern at current suite size.
- **Tick boundary coincidence.** A factory completion landing exactly on a step boundary can be off-by-one. Mitigation: use `runUntil` with a state predicate for these tests, or pick a step that doesn't divide 1.0 evenly (`1/60` doesn't).

---

## Verification

Strategy document with no code yet; verification is staged:

1. **Plan-level check** — write the canonical sample test in our heads ("autopilot delivers `ORBITAL_CONSTRUCTION_PODS` Of_Frame to luna_orbital, orbital becomes operational"), confirm every primitive it needs is described above (fixed-step driver, predicate runner, scenario helper, event bus, recording subscriber, named construction constant). Done.
2. **Migration-step 1 check** — once `EventBus` + orbital events + `TextFormatter` are in, in-game scroll-back text is byte-for-byte unchanged from current `LogSink` output. Confirms the formatter swap is behaviour-preserving.
3. **First behavioural test** — `make tests && ./bin/debug_x64/tests --test-case="*Orbital construction*"`. If writing the test surfaces missing primitives or awkward assertions, this document gets updated before broader rollout.

---

## Critical files to touch

State / event system:
- New: `include/state/events.h` — `EventBus`, `Event` family structs, `EventSubscriber` interface.
- [include/state/game.h](../include/state/game.h), [src/state/game.cpp](../src/state/game.cpp) — own the bus, name the `8` constant.
- [src/state/autopilot.cpp](../src/state/autopilot.cpp), [src/state/shuttle.cpp](../src/state/shuttle.cpp), [src/state/ios.cpp](../src/state/ios.cpp), [src/state/craft.cpp](../src/state/craft.cpp) — publish craft/autopilot events.
- [src/state/factory.cpp](../src/state/factory.cpp), [src/state/research_facility.cpp](../src/state/research_facility.cpp) — publish production/research events.

UI bridge:
- [include/state/log_sink.h](../include/state/log_sink.h) / [include/pages/page_log.h](../include/pages/page_log.h) — `LogSink` stays as PageLog's string-input interface.
- New: `include/pages/text_formatter.{h,cpp}` — consumes events, writes formatted lines to PageLog.
- [include/pages/factory_view.h](../include/pages/factory_view.h) and analogues — opt-in subscriptions for reactive renders, on a later pass.

Page lifecycle (prerequisite for UI subscriptions):
- [include/pages/base_page.h](../include/pages/base_page.h) — add `virtual void deactivate()`.
- [include/pages/pages.h](../include/pages/pages.h), [src/pages/pages.cpp](../src/pages/pages.cpp) — call `deactivate()` on outgoing page in `switchToPage`; add `reactivateCurrent()`.
- [src/main.cpp](../src/main.cpp) — F8 quickload calls `reactivateCurrent()`; remove the `// TODO ugly`.

Tests:
- New: `tests/sim_test_support.{h,cpp}` — `runFor`, `runUntil`, `RecordingSubscriber`, scenario helpers.
- New: `tests/test_simulation.cpp` — behavioural tests.
- [premake5.lua](../premake5.lua) — no change needed; `files {"tests/**.cpp"}` already picks up new files.
