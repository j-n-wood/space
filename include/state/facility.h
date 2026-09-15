#pragma once

#include <memory>
#include <vector>

#include "state/stores.h"
#include "state/factory.h"
#include "state/waypoint.h"
#include "state/location.h"

// A facility IS a place: a child location of the body it sits on or orbits. That
// parent is `Location::primary`; its identity is `Location::id`, drawn from the same
// sequence as bodies; what it is, is `Location::type`.
//
// Facilities are owned by Game::locations like any other location. `Game::bases` and
// `Game::orbitals` are non-owning views for iteration, as `shuttles` and `factories`
// already were.
class Facility : public Location
{
public:
    int faction_id;

    // Where it sits, and so what a craft must do to reach it. Independent of `type`
    // -- a future kind could exist at either surface or orbit without inventing a
    // type per placement.
    SublocationType sublocation;

    Stores stores;
    std::unique_ptr<Factory> factory; // RF bases typically do not have factory, orbitals do
    bool operational;                 // fully constructed
    bool aoc_installed;
    bool sdm_installed;
    bool mtx_installed;
    uint8_t construction_progress; // 0-100, for construction progress of facility, if under construction
    float damage;                  // 0-100, for damage level of facility, if damaged

    // `parent` is the body this facility belongs to. Id and name are assigned by the
    // Game factory that creates it, which owns the id sequence.
    Facility(Location *parent, LocationType t, SublocationType s)
        : Location(parent ? parent->system : nullptr, -1, "", t),
          faction_id{0}, sublocation{s}, operational{false}, aoc_installed{false},
          sdm_installed{false}, mtx_installed{false}, construction_progress{0}, damage{0}
    {
        primary = parent;
        radius = 0.0f; // not drawn or hit-tested in the orrery yet
    }
    virtual ~Facility() {}

    // advance time one tick
    virtual void update();

    Factory *createFactory();
};

// No owning typedef here on purpose: Game::locations owns every facility, as it
// owns every other location. Anything holding a unique_ptr<Facility> would be a
// second owner.