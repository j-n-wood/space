#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../include/loaders/loader.h"
#include "../include/loaders/load_system.h"
#include "../include/state/system.h"
#include <sqlite3.h>

TEST_CASE("Database close should not crash") {
    Loader loader(":memory:");
    CHECK(loader.isValid());
}

TEST_CASE("Database read and close should not crash") {
    Loader loader("./resources/initial.db");
    REQUIRE(loader.isValid());

    SystemPtr system = createSystem(false);
    REQUIRE(system);
    REQUIRE(loadSystem(&loader, 1, system.get()));

    CHECK(system->numPlanets == 12);
}
