# Commands: user interaction as data

> Not started. The inbound mirror of [events.md](../events.md): that document routes
> everything the simulation says to its consumers, this one routes everything its
> consumers say to it.

## Purpose

- **One way in.** Every action that changes game state is a named command with
  identifier arguments, executed by `Game`. The UI stops being the only thing that can
  play.
- **A journal falls out.** Commands stamped with the ticks they landed on are a session
  recording, and `SaveGame` already supplies the starting state they replay against.
- **Agents become possible.** An opponent, a scripted scenario, or an external process
  can submit the same commands a player's click submits, and be held to the same rules.
- **Tests gain reach.** The actions that currently exist only as page code — production
  queueing, training, crew assignment, drone transfer — become callable, and so testable.
- **Affordances get a home.** "What can I do right now" is currently answered in render
  code and nowhere else. It is needed by the UI (greying out buttons), by an agent
  (choosing a move) and by a server (rejecting an illegal one).

## Context

### What is already paid for

This is cheaper than it looks, because most of the hard parts are done:

- **A fixed-tick simulation.** [`Game::tick`](../../src/state/game.cpp#L1249) advances
  `game_tick_count` by `GAME_SIM_TICK`, driven *from* the frame loop but not *by* frame
  time. A command stamped `(tick, seq)` is unambiguous.
- **A deterministic, persisted RNG.** [`Rng`](../../include/state/rng.h) counts draws and
  has `restore(seed, n)`; [`SaveGame`](../../src/loaders/save_game.cpp#L219) and
  [`Loader`](../../src/loaders/loader.cpp#L40) already round-trip both. Only three sites
  draw from it.
- **No wall clock or input inside `state/`.** The only raylib there is `TraceLog` and the
  `Vector2`/`Rectangle` types.
- **Stable integer identity.** Locations and facilities share `Location::id`; crews and
  objects have ids; shuttles and IOS draw from one `craft_max_id`
  ([game.cpp:468](../../src/state/game.cpp#L468),
  [game.cpp:542](../../src/state/game.cpp#L542)) so their ids cannot collide. Only
  `craftByID` is missing.
- **A base state to replay against.** `SaveGame` already writes a complete world, which is
  what a journal needs to start from. The journal itself is a separate file — see
  [The journal](#the-journal).
- **Headless.** The `tests` target ([premake5.lua:287](../../premake5.lua#L287)) builds
  all of `src` except `main.cpp`, links raylib for symbols, and never opens a window.
- **A test driver.** `runFor` in [sim_support.h](../../tests/sim_support.h) already steps
  the sim by whole ticks.

### Where mutations actually live

The premise that most game logic runs through `Game` is about half true. Craft and pod
actions do — `setPodType`, `setSupplyPodContent`, `loadWeapon`, `commissionShuttle`. A
large slice does not, and has no `Game` entry point to route through:

| Action | Site |
|---|---|
| Queue / drop / repeat production | [factory_view.cpp:112-144](../../src/pages/factory_view.cpp#L112-L144) |
| Crew from factory to barracks | [factory_view.cpp:193](../../src/pages/factory_view.cpp#L193) |
| Set research project | [research.cpp:70](../../src/pages/research.cpp#L70) |
| Add / remove trainees, and the population bookkeeping | [training_view.cpp:113-120](../../src/pages/training_view.cpp#L113-L120) |
| Assign crew to craft, crew to engineering | [bay_view.cpp:418](../../src/pages/bay_view.cpp#L418), [bay_view.cpp:429](../../src/pages/bay_view.cpp#L429) |
| Drone transfers, orbital ↔ fleet | [drone_control_view.cpp:173-192](../../src/pages/drone_control_view.cpp#L173-L192) |
| Autopilot flow mode per resource | [autopilot_view.cpp:106](../../src/pages/autopilot_view.cpp#L106) |
| Add derrick | [resources.cpp:24](../../src/pages/resources.cpp#L24) |
| Launch, dock, ascend, descend, drive, scan | [shuttle_view.cpp:174-264](../../src/pages/shuttle_view.cpp#L174-L264) |

**They mutate from `render()`, not `input()`.** raygui is immediate-mode, so the button
and its handler are one expression; most `input()` methods are empty
([factory_view.cpp:45](../../src/pages/factory_view.cpp#L45),
[training_view.cpp:52](../../src/pages/training_view.cpp#L52),
[research.cpp:15](../../src/pages/research.cpp#L15)). This matters less than it sounds —
the handler body builds a command instead of poking state, and stays where it is — but it
means the change touches render code across a dozen files rather than a dozen tidy
`input()` bodies.

### The AI is an agent with a different capability set

[`FactionController::update`](../../src/state/faction_controller.cpp#L107) observes state
and acts every tick. It is already an agent. What it does not have is a legitimate path
for the things it is *supposed* to be able to do that a player is not:

```cpp
o->factory->queueItem(ItemType::Ios_Drone);
o->factory->queue.back().started = true; // hack around actual resource requirements for now
```
[faction_controller.cpp:161-162](../../src/state/faction_controller.cpp#L161-L162)

It also does `i->pods[0].amount += reinforce_amount` to put drones straight into a fleet,
where a player must fly a craft to an orbital and transfer them by hand. **That asymmetry
is deliberate, not a cheat to be removed.** The problem is only that it is expressed as a
reach into state rather than as a declared capability — so it is invisible to the data, to
the save file, and to anything that wants to reason about difficulty.

This gives the work a sharp acceptance test: **if `FactionController` can be rewritten to
emit only commands and still play, the command set is complete.** That tests the set
against *playing the game* rather than against the shape of the current UI — and it
forces each asymmetry to be named. See [Capabilities](#capabilities), below.

## The model

```cpp
struct Command
{
    CommandType type;
    int actor;      // faction id -- whose capabilities apply
    int subject;    // craft / facility / location id
    int target;     // crew / item / resource / topic id
    int index;      // pod index, queue index
    int amount;
    int flag;
};

bool Game::execute(const Command &c);   // resolves ids, checks legality, applies, journals
bool Game::isLegal(const Command &c) const;
void Game::legalCommands(int actor, std::vector<Command> &out) const;
```

**Every command names its actor.** Legality is then a question about the actor's faction,
not about whether the caller happens to be the UI. That is what lets the opponent and the
player share one execution path while keeping their different powers.

**Every command is stamped `(game_tick, realtime_tick, seq)`.** Two clocks, because the
game clock can be stopped while a countdown runs — see
[Realtime interactions](#realtime-interactions). The pair plus a sequence number gives
every command in a session a total order, which is what a journal needs and what a
second attached client would need.

**Execute immediately; do not queue to the next tick.** Several call sites consume a
return value — `craft = commissionShuttle(facility)` at
[bay_view.cpp:478](../../src/pages/bay_view.cpp#L478) is the clearest. The main loop
already advances ticks *after* input and render, so every command raised in a frame
naturally lands on one tick boundary. Deferred execution would force asynchrony through
the UI for no gain in replay fidelity.

**Pointers become ids at the boundary.** Call sites hold `Craft*`, `Facility*`, `Crew*`;
a journalled or transmitted command carries `craft_id`, `location_id`, `crew_id`,
`pod_index`. Mechanical, but it is the per-command cost that makes thirty-five commands
feel like thirty-five commands.

**Navigation is not a command.** Page switches and `ViewState` focus are not game state
and must not be journalled. Consequently commands carry their own context rather than
reading "the current facility" — unlike
[`processConsoleCommand(cmd, l, f)`](../../src/state/game_console_commands.cpp), which is
otherwise the working precedent for naming a mutation from outside.

## Capabilities

Asymmetries between what the opponent can do and what the player can do become **named,
persisted faction capabilities**, checked where the rule lives, rather than reaches into
state from the controller.

The pattern already exists. `Faction::orbital_drone_control`
([faction.h:29](../../include/state/faction.h#L29)) is a bool on the faction, a column in
the `factions` table read by [`Loader`](../../src/loaders/loader.cpp#L56) and written by
[`SaveGame`](../../src/loaders/save_game.cpp#L307), consulted at the one place the rule
bites ([craft.cpp:268](../../src/state/craft.cpp#L268)): an opponent orbital defends
itself with drones, a player's cannot. Nothing about that is a special case in the
controller — it is a property of the faction, in the data.

The controller's current reaches become the same shape:

| Reach today | Capability |
|---|---|
| `queue.back().started = true` — production without the resources | `unconstrained_production` |
| `pods[0].amount += n` — drones straight into a fleet | `direct_drone_deployment` |
| `spawnWarship` from accumulated drones | `warship_spawning` |

Three consequences, all of them wanted:

- **The opponent goes through `execute()` like everything else.** `isLegal` consults
  `factions[c.actor]`, so one path serves both sides and the difference is data.
- **Difficulty becomes tunable without code.** Escalation can grant capabilities as the
  opponent's `captured_orbitals` climbs, next to the existing `warshipRank` and
  `garrisonThreshold` ladders.
- **Capabilities can be unlocked for the player.** Not initially, but the slot is there,
  and the game already does this: the AOC is an item that, once produced and installed,
  frees a factory from needing a crew ([factory.cpp:56-64](../../src/state/factory.cpp#L56-L64)).
  A capability earned through play is the same move at faction scope.

The acceptance test is then stronger than "the AI still works": **every advantage the
opponent has is enumerable from the save file.**

## Policies

Standing orders, evaluated inside the simulation, that answer a situation on an actor's
behalf when that actor is not a person watching a screen.

This is the answer to realtime events for agents, and it preserves the mechanism rather
than dissolving it — see [Realtime interactions](#realtime-interactions) for the clock
they are evaluated against and what remains undecided. A policy resolves to a command, so it adds no verbs; it only
chooses among them without a human in the loop.

The precedent is the autopilot. `Autopilot::flow[ResourceType::Count]`
([autopilot.h:44](../../include/state/autopilot.h#L44)) is a declarative per-resource
standing order — load at source, load at destination, balance — persisted with the craft
and evaluated tick by tick with nobody watching. A reaction policy is that idea applied to
events instead of resources:

| Situation | Policy choices |
|---|---|
| Craft under attack | flee / commit *n* drones / do nothing |
| SDM armed on my facility | abort / allow |
| Boarding action | resist / surrender |

Where a policy lives is the design question: on the faction (one doctrine for everyone),
on the craft or facility (per-asset orders, matching how the autopilot is scoped), or
both, with the asset overriding. The autopilot's scoping suggests per-asset with a faction
default.

**Policies are for agents only.** For a player, reacting *is* the mechanic. A player-facing
automation on the AOC model — *disable an armed SDM within five seconds* — is imaginable
and the mechanism would support it, but it is **not on the roadmap** and nothing here
should be built to accommodate it.

**This is also what makes the opponent's reactions legible.** Today the opponent does not
react to realtime events at all; the events exist to pressure the player. A policy gives
the other side the same vocabulary, in data.

## The command surface

Roughly 40 state-mutating interaction sites out of 77 widgets and hotkeys, collapsing to
**30–35 command types** in eight families:

| Family | Examples |
|---|---|
| Craft movement | launch, dock, ascend, descend, engage/disengage drive, set destination |
| Pod loading | set type, set supply/tool/cryo content, unload all, unload object, load weapon |
| Craft lifecycle | commission shuttle, commission IOS, activate pod, start scanning, trade |
| Autopilot | engage, disengage, set flow mode per resource |
| Factory | queue item, drop queue item, toggle repeat |
| Facility | add derrick, activate/deactivate SDM |
| Crew | assign to craft, to engineering, to research, return to barracks, add/remove trainees |
| Combat | add drone to fleet, add all, return drone, flee |

Plus event response — `applyEvent(event, craft, proceed)` — which is the one command whose
*absence* blocks the clock. See below.

## The journal

**JSONL, in its own file, paired to the save it starts from.** Not a table in the save
database: a journal is an append-only stream, a save is a snapshot, and they have
different lifetimes. One line per command, written as it executes.

The format is chosen to be the same one the wire uses. A journal line *is* an agent
protocol message — phase H sends exactly these objects over stdio — so there is one
format to define, one parser, and a recorded session can be piped into a live simulation
without translation.

```jsonl
{"v":1,"base":"quicksave.db","session":"5f3a…","from_tick":0}
{"gt":1000,"rt":0,"seq":0,"cmd":"queue_item","actor":0,"subject":12,"target":7}
{"gt":1000,"rt":200,"seq":1,"cmd":"flee","actor":0,"subject":42}
```

**Pairing is the cost of the split**, and has to be deliberate: a journal replayed against
the wrong world fails silently and confusingly. The header line names the base save, and
the save gets a `session` id in its `game` table so the pair can be checked rather than
assumed. A content hash of the database would be stronger still if drift turns out to
matter in practice.

Worth adding cheaply at the same time: **periodic checkpoint lines** carrying a state hash
at a given tick. A divergent replay then fails near the command that caused it instead of
at the end, which is the difference between a usable tool and an unusable one.

## Queries are the other half

For any input mechanism that is not a person looking at a screen, commands alone are
useless. The query side is mostly **relocation**, not new work: every page is already a
projection of state scoped to a decision.

| Page | Query it implements |
|---|---|
| [MasterControlView](../../src/pages/master_control.cpp) | fleet and orbital status across a system — closest to an agent's `observe()` |
| [FactoryView](../../src/pages/factory_view.cpp) | what can be built, what is queued, which requirements are short |
| [StoresView](../../src/pages/stores_view.cpp) | inventory |
| [BayView](../../src/pages/bay_view.cpp) | craft configuration, crew availability |
| ResearchView / TrainingView | progress and capacity |
| SystemView / orrery | spatial layout |

**The legal-move API is half built.** Already callable: `canCommissionShuttle`,
`canCommissionIOS`, `canActivatePod`, `canPerformTrade`, `canDock`, `canLaunch`,
`canAscend`, `canDescend`, `canEngageDrive`, `canEngageAutopilot`, `canAddDerrick`,
`Factory::canBuild`. Still stuck in render code: which build requirements are short
([factory_view.cpp:75](../../src/pages/factory_view.cpp#L75), as a text colour) and the
enable/disable reasoning at
[bay_view.cpp:464-469](../../src/pages/bay_view.cpp#L464-L469), which picks the button's
tooltip from the same condition that greys it out.

Finishing that enumeration is the highest-value single piece for agent play — higher than
the journal. An agent made to guess verb names and entity ids flails; an agent handed
"here are the fourteen legal moves and their arguments" plays competently at once. The
same function serves three consumers: the UI greys out a button, an agent picks a move, a
server rejects an illegal one.

## Phases

| | Work | Est. |
|---|---|---|
| A | Extraction — every UI-reachable mutation becomes a `Game::` verb | 3–4 d |
| B | Affordance / legal-command enumeration, actor-scoped | 2–3 d |
| C | `Command` type, dispatch by name and id, `craftByID` | 1.5–2 d |
| D | Headless target, `step` / `runUntil` driver | 0.5–1 d |
| E | Faction capabilities — three flags, columns, checks at the rule sites | 0.5–1 d |
| F | Convert `FactionController` to the command path — the acceptance test | 1–2 d |
| R | Realtime sub-tick clock, plus the three determinism fixes it depends on | 2–3 d |
| M | Battle modal — locks the UI until the battle resolves | 1–2 d |
| G | Observation projection, actor-scoped | 3–5 d |
| H | stdio JSON protocol for an external agent | 2–3 d |
| P | Reaction policies, once their scope is settled | 2–4 d |
| — | Journal and replay — JSONL writer, pairing, replay runner — once C exists | 1–2 d |

**A through F is about 9–13 days** and yields in-process agents, scripted scenarios and
the testing reach. G and H add an external agent for another 5–8. Each phase is useful on
its own, and **A is worth doing even if nothing else follows** — the actions it extracts
are precisely the ones no test can currently reach.

**E is cheap and worth pulling forward.** The three capability flags are a day's work on
an established pattern, and doing them before F means the controller has somewhere
legitimate to put its advantages at the moment it is converted, rather than having them
stripped and reinstated.

**P is gated on design, not effort.** It is listed late because the questions under
[Realtime interactions](#realtime-interactions) decide its shape, not because
it is hard; an agent playing a session with any realtime event in it needs P, so it comes
before H in practice.

**R is where the drone battle gets fixed**, and most of its cost is moving `fleetCombat`
out of the view — work phase A has to do anyway. The rest is an accumulator, a counter and
a constant. Note that R pays for itself without the journal: it is what makes a battle
reproducible, and therefore testable, at all.

**M is a UI feature, listed here because R depends on it.** It is mostly reuse of the
existing modal stack; the real work is separating modality from the clock policies it is
currently bundled with — see [Realtime interactions](#realtime-interactions) — without
which the battle freezes the moment it becomes modal.

Phase A can proceed page by page with no new abstractions, so it is interruptible.

## Open: time advancement control

Deferred for its own review. The problem is not the mechanism but the authority.

**Today the clock belongs to the UI.**
[`PageManager::advanceGameTime`](../../src/pages/pages.cpp#L233) is the gate, and it reads
`KEY_SPACE` directly; `main.cpp` pumps ticks only when it returns true. `time_rate`,
`paused`, `auto_advance_time` and `manual_advance_time` all live outside `Game`. A second
input source cannot be added without deciding who owns the clock.

Candidate regimes, over the same mechanism (not to be confused with the reaction
[policies](#policies) above — this is about who drives the clock, not who answers an
event):

1. **Client-paced** (today). The attached UI pumps ticks at wall rate. Correct for one
   local player; meaningless with two.
2. **Sim-paced.** The clock runs regardless and inputs keep up or miss their window.
   Required for adversarial real-time multiplayer; hopeless for an agent that thinks for
   seconds.
3. **Lockstep.** The clock advances when every attached input has submitted for the tick.
   What tests and agents want; needs a timeout story for a human who walks away.
4. **Explicit step verbs.** `step(n)` and `runUntil(predicate)` as commands. Already the
   test shape (`runFor`) and already proposed as Principle 6 of
   [events.md](../events.md) — which means **the agent driver and the test harness are
   the same component**, a real consolidation argument.

The likely resolution is that tick advancement becomes a command like any other, and
1–4 become regimes a session picks. Client-side pacing (`time_rate`, the Space key)
then stays client-side and never enters the protocol. To settle: what happens when a
human and an agent are attached to one session.

**Whatever is chosen here does not govern realtime events.** They advance on wall-clock
independently of the tick gate, by design — see below. A session can be lockstepped or
stepped by an agent and the SDM countdown still runs at one second per second for anyone
watching it.

**`notify_player` is the interesting case.** The sim raises an event, `PageManager`'s
sink handlers stop the clock ([pages.cpp:154-166](../../src/pages/pages.cpp#L154-L166) for
craft-under-attack, SDM activation, boarding), and it resumes only on acknowledgement.
That is a clean mechanism sitting in the wrong class. Generalised, it is "the session
clock halts pending a decision from whoever owns the affected thing" — which is exactly
what an agent needs as an interrupt, and exactly what a server needs for a client that
has gone quiet.

## Realtime interactions

**Realtime events are a mechanic, not an accident of the implementation**, and the design
must preserve them. The resolution is a second clock, not a different one.

### Terminology

Two different things run on wall-clock time, and calling both "realtime events" has
already caused confusion in this document:

- **A realtime event** is the [`RealtimeEvent`](../../include/state/realtime_event.h)
  hierarchy — today `SDMEvent` and `CraftUnderAttackEvent`. Sim-raised, held in
  `Game::realtime_events`, advanced by `Game::advanceRealTime`, resolved by
  `completeRealtimeEvents`.
- **A battle** is the drone engagement: `DroneControlView` in state `DCS_BATTLE`,
  advanced by the page's `update(delta)`. **There is no `RealtimeEvent` class for it**,
  and it is not raised by the simulation — the player enters it by activating the drone
  computer on a craft carrying a weapon pod
  ([shuttle_view.cpp:219-221](../../src/pages/shuttle_view.cpp#L219-L221)).

They are also different *subjects*, which is why they do not co-occur today:

| | Attacker | Defender | Initiated by |
|---|---|---|---|
| `CraftUnderAttackEvent` | an AI warship ([craft.cpp:506-511](../../src/state/craft.cpp#L506-L511)) | a craft | the simulation |
| battle | the player's drone fleet | a hostile **orbital's** garrison ([`hostilesAt`](../../src/state/game.cpp#L1662) tests for an orbital) | the player |

**They cannot overlap, for two independent reasons**, both already in place:

- **Structurally.** `targetCraftAt` excludes warships
  ([game_faction.cpp:31](../../src/state/game_faction.cpp#L31)), and `isWarship()` is
  "has a weapon pod with drones" ([craft.cpp:929](../../src/state/craft.cpp#L929)) — the
  same condition that offers the drone computer in the first place. So the craft that can
  enter a battle is exactly the craft an AI warship will not auto-engage. Two warships
  face off and wait for the player to decide. (The TODO at
  [craft.cpp:505](../../src/state/craft.cpp#L505), *"if they are warships - player
  controls engagement"*, describes behaviour that is now implemented at both ends; it
  reads as stale.)
- **Temporally.** With the battle modal stopping the game clock, no tick runs, so neither
  `FactionController::update` nor the per-craft AI at
  [craft.cpp:496](../../src/state/craft.cpp#L496) executes. Nothing is in a position to
  start an engagement while a battle is on.

The structural reason is the load-bearing one: it holds whether or not the clock stops.

**Both advance on wall-clock, so both belong on the realtime tick below.** Whether the
battle should also *become* a `RealtimeEvent` is taken up at the end of this section.

[`Game::advanceRealTime`](../../src/state/game.cpp#L56) advances
[`RealtimeEvent`](../../include/state/realtime_event.h) on wall-clock delta. Crucially it
is gated only on a modal being open, while the tick loop is additionally gated on
`advanceGameTime()` — so **releasing Space stops the simulation but not the countdown.**
A [`CraftUnderAttackEvent`](../../src/state/craft_under_attack_event.cpp) deals damage at
four seconds and destroys the craft at expiry whatever the game clock is doing; an
[`SDMEvent`](../../src/state/sdm_event.cpp) destroys the facility unless `sdm_active` is
cleared in time. The player cannot buy thinking time by stopping the clock. That pressure
is the point.

So re-anchoring these to the *game* clock is off the table: it does not merely lose
texture, it removes the mechanism.

### The sub-tick realtime clock

Give the realtime layer **its own fixed-rate clock**, at the same rate as the simulation,
accumulated from wall-clock in normal play exactly as `main.cpp` already accumulates
`GAME_SIM_TICK`:

```cpp
constexpr double REALTIME_TICK = 1.0 / 64.0;  // its own constant: same rate, different clock
uint64_t realtime_tick_count;                 // free-running within a session; not saved
```

`RealtimeEvent::update` then counts whole realtime ticks instead of taking a float delta,
and the accumulator carries the same spiral-of-death guard as the sim loop. Normal play is
unchanged — the countdown still runs at one second per second, still runs while the game
clock is stopped, still cannot be escaped by releasing Space.

**This makes every command placeable.** Stamp commands `(game_tick, realtime_tick, seq)`
and arbitrary-timing reactions become exactly recordable: *fled the battle at realtime
tick 200 within game tick 1000*. Apply the stamp to **all** commands, not only reactions —
it also resolves the case where the game clock is stopped and several ordinary commands
would otherwise share one `(tick, seq)` with no way to order them against an event in
flight. One free-running counter gives everything a total order.

It also supersedes the earlier idea of recording each event's `time_remaining` alongside
the command. That is unnecessary: remaining time is derivable from the event's start tick
and its duration, both of which replay already has.

### Recompute by default, record outcomes as the fallback

A fixed realtime tick converts most of what would have to be *recorded* into something
that can be *recomputed*, which is strictly better — a recomputed outcome cannot drift out
of step with the state around it. The rule:

- **Event starts.** Sim-caused starts (a warship engaging) follow from the simulation and
  need nothing. A start that does not — one seeded externally — is recorded as a
  pseudo-command at its `(game_tick, realtime_tick)`.
- **Event ends.** Derivable from start tick, duration, and whatever commands or policies
  intervened.
- **Outcomes.** Recorded only where a source of nondeterminism is deliberately kept
  outside the sim, and flagged so replay asserts the recorded value instead of computing
  one. This is the escape hatch, not the normal path.

### What has to be fixed for recompute to work

The realtime layer is not merely unrecordable today — **it is not deterministic at all**,
for three separate reasons. All three are worth fixing on their own merits, which is the
main argument for doing this work.

1. **Variable delta.** [`updateRealtimeEvents`](../../src/state/game.cpp#L1929) passes the
   raw frame delta down. In `fleetCombat` the accumulated strength crosses the casualty
   threshold in frame-sized jumps, and each casualty changes `live_count`, which changes
   the accumulation rate — so casualty timing feeds back on itself and the battle resolves
   differently at different frame rates. A fixed tick removes this.
2. **A second, unpersisted RNG.** `fleetRng()`
   ([fleet_movement.cpp:39-44](../../src/pages/fleet_movement.cpp#L39-L44)) is a
   function-local `static std::mt19937` seeded `0xD5049Eu`, used by `killRandom` to choose
   which drones die. Its comment — *"No RNG exists elsewhere in the codebase"* — is simply
   wrong: [`Game::rng`](../../include/state/rng.h) exists, counts its draws, and is
   persisted. Routing `killRandom` through it is a small change and a prerequisite for
   reproducing any battle.
3. **Combat lives in the view.** `fleetCombat`
   ([drone_control_view.cpp:269](../../src/pages/drone_control_view.cpp#L269)) is driven
   from `DroneControlView::update`, which is called from `ShuttleView::update`
   ([shuttle_view.cpp:508](../../src/pages/shuttle_view.cpp#L508)) — so **a battle only
   resolves while the player has that page open.** That is a correctness bug before it is
   a replay problem. The combat model has to move into `state/`, leaving the view with the
   drawing and the input.

Fixing these also makes the drone battle unit-testable, which it currently cannot be at
any price. The win is independent of whether a journal is ever written.

### Realtime events are not persisted

**Save granularity stays at game ticks.** A realtime event is a live situation, never
written to a save file, and the realtime tick counter does not need persisting either.

**What a save does to a live event is per event type**, not one global rule, because the
two existing events mean different things:

| Event | On save | Why |
|---|---|---|
| `SDMEvent` | completes | Ignoring an armed SDM is *supposed* to detonate it. The countdown is time-limited by design, so resolving it is a legitimate outcome rather than a penalty for saving. |
| `CraftUnderAttackEvent` | blocked | Losing the craft because you pressed F5 is not an outcome anyone chose. |

So `RealtimeEvent` grows a `virtual bool blocksSave() const` — true for the attack, false
for the SDM — and the F5 path consults it instead of flushing everything. A battle is not
a `RealtimeEvent`, so it is blocked by its modal rather than by this flag; if it ever
becomes one, the flag covers it too. Today it flushes
everything: the handler calls `completeRealtimeEvents()`
([main.cpp:387](../../src/main.cpp#L387)) "to ensure all realtime events are completed
before saving", and [`completeRealtimeEvents`](../../src/state/game.cpp#L1289) calls
`onComplete()` on every live event, which destroys the craft for a
`CraftUnderAttackEvent`. **Quicksaving during an attack currently kills the craft.** The
same call sits at the head of `tick()` ([game.cpp:1251](../../src/state/game.cpp#L1251));
there it is deliberate pressure — a countdown cannot be skipped by advancing time — and it
stays.

Note that `RealtimeEventType::Facility_Under_Attack` is declared
([realtime_event.h:8](../../include/state/realtime_event.h#L8)) with no class and no
caller. When it arrives it needs both answers decided: does it lock the UI, and does it
block a save.

### The battle locks the UI

Rather than deferring a save request, **a battle takes a modal that must be resolved —
fled, or fought to a finish — before anything else unlocks.** Queued as a UI feature. The
save question then answers itself: F5 is unreachable while the modal is up, so
`blocksSave()` is a backstop rather than the mechanism.

**Modal here means focus, not time.** A modal form holds input and must be closed before
another form opens. Whether the clocks run while it is open is a separate property that
each modal should declare, and the two are currently conflated: `isModalActive()`
([pages.cpp:193](../../src/pages/pages.cpp#L193)) is just `!modals.empty()`, and it is
consulted for four unrelated purposes.

| What `isModalActive()` drives today | Is it modality? |
|---|---|
| raygui control lock, `ControlLockToggle lock(isModalActive())` ([pages.cpp:81](../../src/pages/pages.cpp#L81)), and the `currentPage->input()` gate in `main.cpp` | yes — input capture |
| `switchToPage` refuses while set | yes — focus retention |
| `advanceGameTime()` returns false ([pages.cpp:237](../../src/pages/pages.cpp#L237)) | no — a time policy |
| `advanceRealTime()` returns false, feeding `game->paused` ([pages.h:134](../../include/pages/pages.h#L134)) | no — a time policy |

So `Modal` grows two declarations, both defaulting to today's behaviour, and the two
`PageManager` accessors consult `getActiveModal()` — already available
([pages.cpp:214](../../src/pages/pages.cpp#L214)) — rather than mere existence:

```cpp
virtual bool pausesGameTime() const { return true; }
virtual bool pausesRealtime() const { return true; }
```

**The battle modal is exactly the case one flag cannot express.** It needs the game clock
*stopped* — `tick()` resolves realtime events at its head
([game.cpp:1251](../../src/state/game.cpp#L1251)), so letting game time run during a
battle would resolve any live attack event instantly — and the realtime clock *running*,
because that is what the battle advances on. A message box keeps both true and nothing
about it changes.

**Modals can open modals**, so the stack is legitimate and the query is over the stack,
not over a single modal. Taking the top — what `getActiveModal()` already returns
([pages.cpp:214](../../src/pages/pages.cpp#L214)) — gives the focused form the say. The
alternative, strictest-wins, differs only when a message box opens over a battle: top of
stack lets the clock policy change under the battle, strictest-wins freezes it. Neither
case arises yet, but the rule should be chosen deliberately rather than falling out of
whichever is written first.

One more gap: **F5 is not modal-gated.** In `main.cpp` the modal check wraps only the
F1–F3 page switches; the quicksave branch sits outside it. It needs the same gate.

**This does not constrain the journal.** A journal replays from a save taken outside a
battle and re-simulates the battle from there, deterministically, given the fixes above.
So a journal may contain battles even though a save may not be taken during one. The two
rules are about different artefacts.

### Still open

1. **Policy scope.** Per-faction doctrine, per-asset standing orders, or both with the
   asset overriding. The autopilot argues for per-asset with a faction default.
2. **When a policy is consulted.** At event raise is simplest. Re-evaluation mid-countdown
   — committing more drones as the battle turns — is richer, and the sub-tick clock makes
   it *representable* (a policy can fire at a realtime tick like any command), so the
   question is now one of design rather than feasibility.
3. **What "fought to a finish" means for the modal.** Fleeing resolves it; so does one
   fleet reaching zero. Whether a stalemate can persist indefinitely, and what the modal
   offers if so, is undecided.

4. **Whether a battle should become a `RealtimeEvent`.** Making it one would put it on the
   same clock, under the same `blocksSave()` rule and the same policy hook as the others,
   and move it into `state/` where phase R needs it anyway. Against: it is player-entered
   rather than sim-raised, and its decision surface is continuous rather than a single
   yes/no, so it may not fit the interface the other two share. Phase R has to answer
   this, because moving `fleetCombat` out of the view is where the question bites.
**The drone battle remains the case that drives the rest.** It mutates
`craft->pods[0].amount` and orbital stores directly from the view
([drone_control_view.cpp:80](../../src/pages/drone_control_view.cpp#L80)), carries all
three determinism defects above, and is the only one of these with a continuous decision
surface rather than a single yes/no. Converting it settles most of the questions above by
example.

## Deferred

- **Removing `Game::getCurrent()`.** 84 call sites, around half inside `state/` itself
  (`craft.cpp`, `factory.cpp`, `training_facility.cpp`, `faction_controller.cpp`). Hosting
  two simulations in one process needs this gone. Doing it speculatively is weeks of churn
  across every file for a capability that may never be built — defer until a second client
  is actually wanted.
- **Fog of war.** Every query currently sees everything. `ViewState::setFactionId` exists
  for testing but does not scope reads. Irrelevant for collaborative play on one faction,
  a real feature for adversarial.
- **Event transport.** [`EventSink`](../../include/state/event_sink.h) pushes to raw
  in-process pointers. A remote client needs per-client serialization and buffering. The
  interface shape is already right; only the transport is missing.
- **Command validation as a trust boundary.** Local commands can be trusted; remote ones
  cannot. This is the affordance work of phase B applied on the receiving side, so it
  costs nothing extra once B exists — but it has to actually be wired in before a socket
  is opened.
- **Float determinism across platforms.** Fine for replaying a journal against the same
  binary. Now that there is a Linux build, comparing journals across toolchains is a
  different and much stronger claim. Decide which kind of replay is wanted before relying
  on it.
- **An agent as a balance instrument.** [todo.md](../../todo.md) asks *"enough to do? Lots
  of waiting at the start."* An agent that plays ten thousand ticks and reports idle time,
  resource bottlenecks and time-to-first-orbital answers that repeatably. Reachable at
  phase H, and likely to earn its keep on pacing feedback long before it earns it on
  finding bugs.
- **Capabilities as difficulty escalation.** Granting the opponent a capability as
  `captured_orbitals` climbs, beside the existing `warshipRank` and `garrisonThreshold`
  ladders, is the obvious next use once the flags exist. Needs the escalation curve
  designed, not just the mechanism.
- **Capabilities as player unlocks.** The AOC shows the shape — an item that, once
  installed, removes a requirement. Which of the opponent's capabilities a player should
  ever earn is a game-design question the command work only makes expressible.
- **Player-facing reaction policies.** *Not on the roadmap.* An automation that disabled an
  armed SDM within five seconds, say, would fall out of the policy mechanism at no extra
  cost — but automating a reaction trades away the mechanic the realtime events exist to
  create, and it is listed here only so the idea is recorded rather than rediscovered.
