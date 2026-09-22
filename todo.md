# TODO

## EC training button

* all main pages have same buttons, with EC having two extras instead of orbital/faction indicator

## staff model

* on craft
* at factories
* cryo pod to transport (if not ships crew)
* transfer at docks

Need:

* class (id, type, leader name, rank, crew size)
* persistence
* craft crew reference
* factory crew reference
* research facility crew reference

Avoid persistence of training facility state by putting on the crew - make rank 0 trainee, and have FT update until rank 1 experience is reached.

Need to disband crew via Game.

## Craft persistence

* scg - need graphics

## factions

* control of orbitals, craft
* visibility in views

## locations

* asteriod locations? Generic belt, major asteriods?

### AMA

### Grapple

In progress - need graphics, click to remove content.

## bay view animation

* use local copy of e.g. pod state to animate transitions. Data is updated immediately.

## fullscreen resize

## production details

* need restrictions, tech level speed

## seam model

* resource abundance, survey delay.

## self destruct

## Capture of orbitals

In progress.

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

## Scanning tools / craft scan state

Grapple and AMA scan space around a craft for _something to interact with_.
The old version would bring up a panel for the tool and allow time to advance.
Moving to other screens closed that panel.
Things like asteroids changed the 'nearby object' every 8 ticks.

Proposal: rather than a tool state that is not 'working', add a craft state for
'scanning'. If enabled, perform location-dependent scanning (some locations
have junk in orbit that can be grappled, asteriods will iterate through
random asteroid as available to grapple or mine).

If a suitable target is nearby, pods with tools that can interact with that
will be available to activate (and optionally work for some time).

Different craft at a location can have different current 'scan targets' (asteroids etc) indicating that they are scanning independent locations.

### Possible scan targets

* collectable items (story items, marked as being on specific body locations)
* different size and composition asteroids

The collectible items must be persisted - they are to be located and collected. They could be located and NOT immediately collected, so must persist.

Asteroid scanning _suggests_ persistence to avoid 'instant' rescan via save-scumming. Fully persisted asteroids are a nice idea, but could create 100s of body locations if done up front.

The model in the game is more a size/primary resource pair. The size is an enum that maps to tonnage. The resource is just an existing resource ID. As craft independently scan, this can differ per craft.

#### Story items

Should be persisted in a dedicated table. Attached to a location by ID, with a type. That is sufficient for now.

#### Craft-related asteroids

Also should be persisted so state is reconstructed. If not made into a location, the minimal values would be a table of 'asteroids' (don't count as locations), with craft_id, size, and resource_id. Size need not be an enum now - we can just use an integer, the only meaning to it is there is a max size that can be grappled, and a minimum size to attach and mine.

### Grapple implementation

Grapple currently holds one of two kinds of items:

* story items, generated and stored in their own table or by code events
* asteroids

The grapple must be able to store the fact that it holds an asteroid of _n_ tonnes of resource _r_, or story item _x_.

Perhaps the simplest thing is to define the table of _objects_, which are the persistent story items and _known asteroids_ i.e. found by scanning.

The grapple pod can then use a property to refer to the ID of the object being held. A new property for this purpose is simple enough, even if mainly unused.

This reflects a 'real object' model. Objects have a type, and other properties (quantity and resource_id, if applicable).

The scan state can be persisted by relating an object to a craft (currently scanning object _x_).

Transient objects (asteroids) can be updated with altered properties. This saves lifetime management. When a craft leaves an 'asteroid belt' location, any related 'scan target object' can be dropped. Held objects are retained, as are story objects (i.e. only type asteroid objects are candidates to be
dropped, and not if grappled).

This would be implemented by an event added when a location is left, that removes any transient 'scan target asteroid' listed for a craft. Other objects are not asteroid type, and grappled objects are not 'scan targets' as the grapple will operate immediately and claim the object ID, removing the scan target ID from the craft.