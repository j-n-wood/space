# Separate facility type from sublocation

> **Status: still a prerequisite, with one adjustment.**
> [facilities_as_locations.md](facilities_as_locations.md) extends `LocationType` with the
> facility kinds, so the discriminator below should be **`LocationType`, not a new
> `FacilityType` enum**. The numbering already lines up —
> `FT_RESOURCE`/`FT_ORBITAL`/`FT_EARTH_CITY` become `LOCATION_TYPE_RESOURCE_FACILITY` /
> `LOCATION_TYPE_ORBITAL` / `LOCATION_TYPE_EARTH_CITY` (the last already exists and is
> unused) — so the field becomes `Location::type` when facilities become locations, with no
> churn. Read `FacilityType` below as "the extended `LocationType`".
>
> Everything else here is unchanged and still needed: dropping `SLOC_EARTH_CITY` from
> `SublocationType`, keeping `sublocation` an independent stored field, and replacing
> `saveBase`'s `training_facility || research_facility` inference with a stored value.

## Context

`SLOC_EARTH_CITY` is not a location — it is a class discriminator wearing a
`SublocationType`. `facilities.type` is written as a sublocation
([save_game.cpp:479](../../src/loaders/save_game.cpp#L479),
[:535](../../src/loaders/save_game.cpp#L535)) and read back to choose which class to
construct ([loader.cpp:132-158](../../src/loaders/loader.cpp#L132-L158)). Earth City is a
`ResourceFacility` subclass that happens to carry extra capabilities; where it sits is the
surface, same as any other resource facility.

**The tag is also inferred rather than stored**, which is a live hazard:

```cpp
// save_game.cpp:470 -- saveBase
if (rf->training_facility || rf->research_facility) // somewhat hacky //TODO
    sublocation = SLOC_EARTH_CITY;
```

`Game::createResearchFacility(ResourceFacility*)`
([game.cpp:169](../../src/state/game.cpp#L169)) accepts **any** resource facility. It is
called only on the Earth City today ([game.cpp:76](../../src/state/game.cpp#L76)), so the
inference holds by luck. Give a plain resource facility a research facility — which the API
permits — and it saves as `SLOC_EARTH_CITY` and loads back as an `EarthCity`, silently
gaining a training facility and a factory. A round-trip changes the object's class.

**Intended outcome:** `FacilityType` says what a facility *is*; `SublocationType` says where
it *sits*. Both are supplied at construction and both are persisted, so neither is inferred
from the other. There is only ever one Earth City, so this needs no defensive machinery — a
plain field with an accessor is enough.

Existing quicksaves are invalidated; only `resources/initial.db` is migrated.

## Design

### `FacilityType`

Values match the existing `type` column, so the bootstrap data keeps its numbers.

```cpp
// include/state/facility.h
enum FacilityType : uint8_t
{
    FT_RESOURCE   = 0,
    FT_ORBITAL    = 1,
    FT_EARTH_CITY = 2,
    FT_COUNT
};
extern const char *FacilityTypeName[FT_COUNT];
```

`SublocationType` loses its third value and becomes what its name says:

```cpp
// include/state/waypoint.h
enum SublocationType
{
    SLOC_SURFACE,
    SLOC_ORBIT,
    SLOC_COUNT
};
```

`SublocationTypeName[]` ([location.cpp:4](../../src/state/location.cpp#L4)) drops
`"Earth City"`.

### Both are constructor inputs

Type is a field rather than a virtual function: the cost is a field read, and it leaves room
to *upgrade* a facility later. It is private with an accessor so it cannot be assigned
casually — construction, or an explicit upgrade path if one ever arrives.

```cpp
// include/state/facility.h
class Facility
{
    FacilityType facility_type; // set at construction; only an explicit upgrade changes it

public:
    int id;
    int faction_id;
    Location *location;
    SublocationType sublocation;   // independent of type -- see note below
    ...

    Facility(Location *l, FacilityType t, SublocationType s)
        : facility_type{t}, id{0}, faction_id{0}, location{l}, sublocation{s},
          operational{false}, aoc_installed{false}, sdm_installed{false},
          mtx_installed{false}, construction_progress{0}, damage{0} {}

    FacilityType type() const { return facility_type; }
};
```

Sublocation stays a separate input rather than being derived from the type. Today the
mapping is total (`FT_ORBITAL`→orbit, everything else→surface), but keeping it independent
is what lets a future facility kind — a listening post, a mass driver battery — exist at
either sublocation without inventing a type per placement. It is also what routes a facility
into the right per-location collection, whether that is a filter or a stored marker.

Subclasses pass their type and default their natural sublocation:

```cpp
// resourceFacility.h
class ResourceFacility : public Facility
{
public:
    explicit ResourceFacility(Location *l, SublocationType s = SLOC_SURFACE)
        : ResourceFacility(l, FT_RESOURCE, s) {}
    ~ResourceFacility();

    bool isEarthCity() const { return type() == FT_EARTH_CITY; }

protected:
    ResourceFacility(Location *l, FacilityType t, SublocationType s)
        : Facility{l, t, s}, num_derricks{0} {}
};

// earth_city.h / .cpp
EarthCity::EarthCity(Location *l, SublocationType s)
    : ResourceFacility{l, FT_EARTH_CITY, s}
{
    training_facility = std::make_unique<TrainingFacility>();
    operational = true;
    construction_progress = 1;
}

// orbital.cpp
Orbital::Orbital(Location *l, SublocationType s) : Facility{l, FT_ORBITAL, s} {}
```

The `sublocation = SLOC_X;` assignments in the three constructor bodies
([resourceFacility.cpp:8](../../src/state/resourceFacility.cpp#L8),
[earth_city.cpp:7](../../src/state/earth_city.cpp#L7),
[orbital.cpp:6](../../src/state/orbital.cpp#L6)) all go — the value arrives through the
initialiser list instead.

### Factory methods carry both through

Defaults keep every runtime call site unchanged; the loader passes what it read.

```cpp
// game.h
ResourceFacility *createResourceFacility(Location *l, SublocationType s = SLOC_SURFACE);
Orbital          *createOrbital(Location *l, SublocationType s = SLOC_ORBIT);
EarthCity        *createEarthCity(Location *l, SublocationType s = SLOC_SURFACE);
```

### Persistence

`facilities.type` keeps its numbers but now means `FacilityType`; a `sublocation` column is
added beside it. Both are read and applied on load.

```cpp
// loader.cpp -- dispatch on FacilityType, pass the stored sublocation through
int type = sqlite3_column_int(query, 3);
SublocationType sub = static_cast<SublocationType>(sqlite3_column_int(query, 4));

Facility *fac = nullptr;
switch (static_cast<FacilityType>(type))
{
case FT_RESOURCE:
{
    auto rf = game->createResourceFacility(loc, sub);
    rf->num_derricks = num_derricks;
    rf->operational = operational;
    rf->construction_progress = construction_progress;
    rf->damage = damage;
    fac = rf;
    break;
}
case FT_ORBITAL:
{
    auto orbital = game->createOrbital(loc, sub);
    orbital->operational = operational;
    orbital->construction_progress = construction_progress;
    orbital->damage = damage;
    orbital->aoc_installed = aoc_installed > 0;
    orbital->sdm_installed = sdm_installed > 0;
    orbital->mtx_installed = mtx_installed > 0;
    fac = orbital;
    break;
}
case FT_EARTH_CITY:
{
    auto ec = game->createEarthCity(loc, sub);
    ec->num_derricks = num_derricks;
    ec->damage = damage;
    fac = ec;
    break;
}
default:
    TraceLog(LOG_ERROR, "Unknown facility type %d for facility %d", type, id);
    return false;
}
```

Save binds the two fields directly. The inference in `saveBase` is deleted:

```cpp
// saveBase
.bind(4, rf->type())
.bind(5, rf->sublocation)

// saveOrbital -- was a hardcoded SLOC_ORBIT
.bind(4, orbital->type())
.bind(5, orbital->sublocation)
```

### Bootstrap migration

`resources/initial.db` keeps its `type` values (0/1/2 already match `FacilityType`) and gains
the new column:

```sql
ALTER TABLE facilities ADD COLUMN sublocation INT;
UPDATE facilities SET sublocation = CASE type WHEN 1 THEN 1 ELSE 0 END;
```

The `CREATE TABLE` strings in `save_game.cpp` (both the schema constant near line 154 and the
two `INSERT` statements) add `sublocation` to match.

## Files

| File | Change |
|---|---|
| [include/state/waypoint.h](../../include/state/waypoint.h) | drop `SLOC_EARTH_CITY`; `SLOC_COUNT` becomes 2 |
| [src/state/location.cpp](../../src/state/location.cpp) | `SublocationTypeName[]` drops its third entry |
| [include/state/facility.h](../../include/state/facility.h) | add `FacilityType`, the private field, `type()`, and the three-argument constructor |
| [src/state/facility.cpp](../../src/state/facility.cpp) | `FacilityTypeName[]` |
| [include/state/resourceFacility.h](../../include/state/resourceFacility.h), [src/state/resourceFacility.cpp](../../src/state/resourceFacility.cpp) | public + protected constructors; `isEarthCity()` reads `type()` |
| [include/state/earth_city.h](../../include/state/earth_city.h), [src/state/earth_city.cpp](../../src/state/earth_city.cpp) | pass `FT_EARTH_CITY`; drop the `sublocation =` assignment |
| [include/state/orbital.h](../../include/state/orbital.h), [src/state/orbital.cpp](../../src/state/orbital.cpp) | pass `FT_ORBITAL`; drop the `sublocation =` assignment |
| [include/state/game.h](../../include/state/game.h), [src/state/game.cpp](../../src/state/game.cpp) | three factory methods gain a defaulted `SublocationType` |
| [src/loaders/loader.cpp](../../src/loaders/loader.cpp) | read `sublocation`; switch on `FacilityType` |
| [src/loaders/save_game.cpp](../../src/loaders/save_game.cpp) | bind `type()` and `sublocation`; delete the inference; schema and both inserts gain the column |
| `resources/initial.db` | the migration above |
| [tests/test_db.cpp](../../tests/test_db.cpp) | the three `dynamic_cast<EarthCity *>` searches (lines 216, 811, 832) can become `b->type() == FT_EARTH_CITY` |

## Knock-on effects

- **`SublocationType(1 - facility->sublocation)`** at
  [game.cpp:289](../../src/state/game.cpp#L289) becomes correct by construction. With two
  values it is a genuine toggle; today it yields `-1` for an Earth City.
- **[facility_lookup.md](facility_lookup.md)'s `sublocationIsSurface()` pairing rule becomes
  unnecessary.** With Earth City at `SLOC_SURFACE`, the per-location filter is a plain
  equality and the trap that plan is built around stops existing. **Sequence this change
  first**, then simplify that plan.
- `craft_destinations.sublocation` values `0`/`1` are unaffected.
- `isEarthCity()` keeps its single consumer at
  [base_page.cpp:91](../../src/pages/base_page.cpp#L91); only its implementation changes.

## Verification

`make && make tests && ./bin/Debug/tests`.

1. **Existing round-trip suite is the safety net.** `test_db.cpp` loads `initial.db`, saves,
   and reloads at lines 188-224, 345-468, 599-672 and 808-840, asserting the Earth City has
   its factory and research facility. Those must pass against the migrated bootstrap file.
2. **Type survives a round-trip.** After save and reload, the facility at Earth reports
   `FT_EARTH_CITY`, the one at Mars `FT_RESOURCE`, and orbitals `FT_ORBITAL`.
3. **The inference bug is gone.** Give a plain `ResourceFacility` a research facility via
   `createResearchFacility`, save, reload: it must come back as `FT_RESOURCE` with no
   training facility. This fails on the current code — it returns as an `EarthCity` — so it
   is the regression test that justifies the change.
4. **Sublocation is independent.** A facility constructed with a non-default sublocation
   keeps it across a round-trip.
5. **Play:** `./bin/Debug/space` — Earth City page reachable with its training and
   research buttons ([base_page.cpp:91-96](../../src/pages/base_page.cpp#L91-L96) drives
   them off `isEarthCity()`), Mars and Jupiter surface and orbital pages behave as before,
   F5/F8 round-trips.
