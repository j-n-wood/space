# TODO

## EC training button

* all main pages have same buttons, with EC having two extras instead of orbital/faction indicator

## staff model

* on craft
* at factories
* cryo pod
* transfer at docks

## Craft persistence

* scg - need graphics

## factions

* control of orbitals, craft
* visibility in views

## locations

* asteriod locations? Generic belt, major asteriods?

### AMA

### Grapple

## bay view animation

* use local copy of e.g. pod state to animate transitions. Data is updated immediately.

## fullscreen resize

## production details

* need restrictions, tech level speed

## seam model

* resource abundance, survey delay.

## self destruct

## Capture of orbitals

## MTX

## orrery position caching

Dropped in the Stage 1 location refactor (`docs/plans/facilities_as_locations.md`). The old
`body_positions[64]` cache existed for two good reasons: it did no position work at all while
time was paused, and it made hit-testing an array read rather than a parent-chain walk. It was
removed because the fixed 64 entries capped a system's body count and were written unchecked,
which blocks facilities becoming child locations.

Current cost per frame, per visible orrery (the destination picker owns a second one):

* draw loop resolves one position per location — Sol is 38 bodies at <= 3 parent hops
* hover test reuses that same value, so it is not a second pass (the old code called
  `renderPosition` twice per body)
* `mouseOverBody()` walks every location, but only on a mouse press, not per frame
* transit lines resolve two positions per IOS in transit

Trivial at this scale, and while time advances it is no worse than before. The regression is
specifically the paused case, which now recomputes every frame.

If it ever matters, re-cache on `Location` rather than in the orrery: a `resolved_position` +
a dirty flag or a `last_resolved_time` stamp, filled by `System::update`. That keeps the
cache growable with the location collection and shared between the two orreries, which the
fixed array could not do.
