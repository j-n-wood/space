// Characterisation test for Stage 1 of docs/plans/facilities_as_locations.md.
//
// Stage 1 moves the orbital elements off System's seven parallel arrays onto
// Location, and replaces index-based position resolution with a walk up the
// `primary` pointer chain. That is a pure representation change, so every body
// must resolve to exactly the same place afterwards.
//
// The golden file is generated on first run and committed. Delete it to
// re-baseline -- which should only be done deliberately, when positions are
// *meant* to move.

#include "doctest.h"
#include "../include/loaders/loader.h"
#include "../include/loaders/load_system.h"
#include "../include/loaders/save_game.h"
#include "../include/state/game.h"
#include "../include/state/system.h"
#include "../include/state/location.h"

#include <cmath>
#include <cstdio>
#include <map>

namespace
{

const char *POS_DB_PATH = "./resources/initial.db";
const char *GOLDEN_PATH = "./tests/expected_positions.txt";

// Fixed sample time so orbital phase is deterministic.
const float SAMPLE_TIME = 1000.0f;

// Orbital radii span 21..720, so resolved positions reach ~1500. A wrong parent
// link moves a body by tens to hundreds of units; float reassociation (the
// recursive sum becomes an iterative one) moves it by ~1e-4 at this magnitude.
// 1e-3 separates those cleanly.
const double POS_TOLERANCE = 1e-3;

struct Sample
{
    double x;
    double y;
};

// The single point that changed in Stage 1: this was System's index-based
// recursion, and is now Location::resolvedPosition().
Vector2 resolvedPositionOf(Location *loc)
{
    if (!loc)
    {
        return Vector2{0.0f, 0.0f};
    }
    return loc->resolvedPosition();
}

std::map<int, Sample> samplePositions(Game *game)
{
    for (auto &sys : game->allSystems())
    {
        sys->update(SAMPLE_TIME);
    }

    std::map<int, Sample> out;
    for (auto &loc : game->allLocations())
    {
        const Vector2 p = resolvedPositionOf(loc.get());
        out[loc->id] = Sample{p.x, p.y};
    }
    return out;
}

bool readGolden(std::map<int, Sample> &out)
{
    FILE *f = std::fopen(GOLDEN_PATH, "r");
    if (!f)
    {
        return false;
    }
    int id = 0;
    double x = 0.0;
    double y = 0.0;
    while (std::fscanf(f, "%d %lf %lf\n", &id, &x, &y) == 3)
    {
        out[id] = Sample{x, y};
    }
    std::fclose(f);
    return true;
}

void writeGolden(const std::map<int, Sample> &positions)
{
    FILE *f = std::fopen(GOLDEN_PATH, "w");
    REQUIRE(f != nullptr);
    for (const auto &entry : positions)
    {
        std::fprintf(f, "%d %.6f %.6f\n", entry.first, entry.second.x, entry.second.y);
    }
    std::fclose(f);
}

} // namespace

TEST_CASE("resolved body positions match the recorded baseline")
{
    Game *game = Game::createCurrent();
    Loader loader(POS_DB_PATH);
    REQUIRE(loader.isValid());
    loader.setGame(game);
    REQUIRE(loader.loadSystems());

    const std::map<int, Sample> actual = samplePositions(game);
    REQUIRE(actual.size() > 100); // sanity: initial.db ships 173 bodies

    std::map<int, Sample> expected;
    if (!readGolden(expected))
    {
        writeGolden(actual);
        MESSAGE("Generated " << GOLDEN_PATH << " with " << actual.size()
                            << " entries. Re-run to assert against it.");
        return;
    }

    CHECK(expected.size() == actual.size());

    for (const auto &entry : expected)
    {
        const auto found = actual.find(entry.first);
        REQUIRE_MESSAGE(found != actual.end(), "location id missing: " << entry.first);

        CHECK_MESSAGE(std::fabs(found->second.x - entry.second.x) <= POS_TOLERANCE,
                      "x drifted for location id " << entry.first);
        CHECK_MESSAGE(std::fabs(found->second.y - entry.second.y) <= POS_TOLERANCE,
                      "y drifted for location id " << entry.first);
    }
}

TEST_CASE("every system resolves its space location")
{
    Game *game = Game::createCurrent();
    Loader loader(POS_DB_PATH);
    REQUIRE(loader.isValid());
    loader.setGame(game);
    REQUIRE(loader.loadSystems());

    // The hierarchy fix-up used to `break` on the first system-0 location, which is
    // body 164 -- so systems 2..9 never had `space` assigned, leaving System::space
    // an uninitialised pointer that Craft::engageDrive then dereferenced.
    REQUIRE(game->allSystems().size() > 2);
    for (auto &sys : game->allSystems())
    {
        CHECK_MESSAGE(sys->space != nullptr, "system has no space location: " << sys->id);
    }
}

TEST_CASE("body count is stable across repeated save and load")
{
    // saveSystem writes every entry of system->locations to `bodies`, and the loader
    // used to re-count them into a pre-sized array. Any divergence between that count
    // and the vector compounded on every cycle. With the parallel arrays gone there is
    // no pre-size to inflate.
    const char *CYCLE_PATH = "./test_positions_cycle.db";

    Game *game = Game::createCurrent();
    Loader loader(POS_DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));

    const size_t original = game->allLocations().size();
    REQUIRE(original > 100);

    size_t previous = original;
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        SaveGame saver;
        REQUIRE(saver.save(CYCLE_PATH) == 0);

        Game *reloaded = Game::createCurrent();
        Loader cycleLoader(CYCLE_PATH);
        REQUIRE(cycleLoader.isValid());
        REQUIRE(reloaded->initialise(&cycleLoader));

        const size_t now = reloaded->allLocations().size();
        CHECK_MESSAGE(now == previous, "body count changed on cycle " << cycle);
        previous = now;
    }
    CHECK(previous == original);

    std::remove(CYCLE_PATH);
}

TEST_CASE("location ids continue the loaded sequence")
{
    Game *game = Game::createCurrent();
    Loader loader(POS_DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));

    // The watermark must match the highest id actually loaded, not the count.
    int highest = -1;
    for (auto &loc : game->allLocations())
    {
        if (loc->id > highest)
        {
            highest = loc->id;
        }
    }
    REQUIRE(highest >= 0);
    CHECK(game->location_max_id == highest);

    // Ids allocated now extend that sequence rather than colliding with it.
    const int first = game->nextLocationID();
    CHECK(first == highest + 1);
    CHECK(game->locationByID(first) == nullptr); // free before use

    System *sys = game->allSystems()[1].get();
    Location *added = game->createLocation(sys, first, "Test Site", LOCATION_TYPE_ORBITAL);
    REQUIRE(added != nullptr);
    CHECK(game->locationByID(first) == added);
    CHECK(game->location_max_id == first);

    const int second = game->nextLocationID();
    CHECK(second == first + 1);
    CHECK(game->locationByID(second) == nullptr);
}

TEST_CASE("location id watermark is rebuilt on load, not persisted")
{
    // Derived rather than stored, so it cannot drift from the data. A fresh game
    // loaded from a save must reach the same watermark as the one that saved it.
    const char *WATERMARK_PATH = "./test_positions_watermark.db";

    Game *game = Game::createCurrent();
    Loader loader(POS_DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));
    const int before = game->location_max_id;
    REQUIRE(before > 0);

    SaveGame saver;
    REQUIRE(saver.save(WATERMARK_PATH) == 0);

    Game *reloaded = Game::createCurrent();
    Loader reloader(WATERMARK_PATH);
    REQUIRE(reloader.isValid());
    REQUIRE(reloaded->initialise(&reloader));

    CHECK(reloaded->location_max_id == before);

    std::remove(WATERMARK_PATH);
}
