#include "doctest.h"
#include "../include/loaders/loader.h"
#include "../include/loaders/save_game.h"
#include "../include/state/game.h"
#include "../include/state/factory.h"
#include "../include/state/orbital.h"
#include "../include/state/stores.h"
#include "../include/state/event_sink.h"
#include <cstdio>
#include <vector>

static const char *FACTORY_DB_PATH = "./resources/initial.db";
static const char *FACTORY_SAVE_PATH = "./test_factory_save.db";

// Records completed production.
class ProductionCapture : public EventSink
{
public:
    std::vector<int> completed;
    void onProductionComplete(Factory *, int item_id) override { completed.push_back(item_id); }
};

// A crewed, fully stocked orbital at Earth on the Game singleton (Factory::update reads
// item definitions and raises events through Game::getCurrent()).
static Orbital *createStockedOrbital()
{
    Game *game = Game::createCurrent();
    Loader loader(FACTORY_DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));

    Location *earth = game->locationByID(4);
    REQUIRE(earth != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    if (!orbital)
    {
        orbital = game->createOrbital(earth);
    }
    REQUIRE(orbital != nullptr);
    REQUIRE(orbital->factory != nullptr);
    orbital->operational = true;
    orbital->sdm_installed = false;
    orbital->mtx_installed = false;
    orbital->factory->aoc_installed = false;

    Crew *engineers = game->createCrew(0, CrewType::Engineer, "Kowalski", 5, 6, 0.0f);
    REQUIRE(engineers != nullptr);
    orbital->factory->crew = engineers;

    for (int r = 0; r < ResourceType::Count; ++r)
    {
        orbital->stores.resources[r] = 100000;
    }
    return orbital;
}

// Tick the factory until its queue empties, with a bound so a stall fails rather than hangs.
static void runQueue(Factory *factory)
{
    for (int tick = 0; tick < 10000 && !factory->queue.empty(); ++tick)
    {
        factory->update();
    }
    REQUIRE(factory->queue.empty());
}

TEST_CASE("Factory::sendToStores installs facility features once, then stores")
{
    Orbital *orbital = createStockedOrbital();
    Factory *factory = orbital->factory.get();
    Stores &stores = orbital->stores;

    SUBCASE("SDM")
    {
        CHECK_FALSE(factory->sendToStores(ItemType::SDM));
        CHECK(orbital->sdm_installed);
        CHECK(stores.items[ItemType::SDM] == 0);

        CHECK(factory->sendToStores(ItemType::SDM));
        CHECK(stores.items[ItemType::SDM] == 1);
    }

    SUBCASE("AOC")
    {
        CHECK_FALSE(factory->sendToStores(ItemType::AOC));
        CHECK(factory->aoc_installed);
        CHECK(stores.items[ItemType::AOC] == 0);

        CHECK(factory->sendToStores(ItemType::AOC));
        CHECK(stores.items[ItemType::AOC] == 1);
    }

    SUBCASE("MTX")
    {
        CHECK_FALSE(factory->sendToStores(ItemType::MTX));
        CHECK(orbital->mtx_installed);
        CHECK(stores.items[ItemType::MTX] == 0);

        // a second MTX is surplus and must not vanish
        CHECK(factory->sendToStores(ItemType::MTX));
        CHECK(stores.items[ItemType::MTX] == 1);
    }

    SUBCASE("features are independent")
    {
        factory->sendToStores(ItemType::SDM);
        CHECK(orbital->sdm_installed);
        CHECK_FALSE(orbital->mtx_installed);
        CHECK_FALSE(factory->aoc_installed);
    }

    SUBCASE("ordinary items go straight to stores")
    {
        CHECK(factory->sendToStores(ItemType::Supply_Pod));
        CHECK(stores.items[ItemType::Supply_Pod] == 1);
        CHECK_FALSE(orbital->sdm_installed);
        CHECK_FALSE(orbital->mtx_installed);
        CHECK_FALSE(factory->aoc_installed);
    }
}

TEST_CASE("Factory::sendToStores without a facility or stores")
{
    Stores stores;

    SUBCASE("no facility: features go to stores")
    {
        Factory factory(nullptr, &stores);
        CHECK(factory.sendToStores(ItemType::SDM));
        CHECK(factory.sendToStores(ItemType::AOC));
        CHECK(factory.sendToStores(ItemType::MTX));
        CHECK(stores.items[ItemType::SDM] == 1);
        CHECK(stores.items[ItemType::AOC] == 1);
        CHECK(stores.items[ItemType::MTX] == 1);
        CHECK_FALSE(factory.aoc_installed);
    }

    SUBCASE("no stores: ordinary item is refused")
    {
        Factory factory(nullptr, nullptr);
        CHECK_FALSE(factory.sendToStores(ItemType::Supply_Pod));
    }
}

TEST_CASE("Factory::update installs a produced feature instead of storing it")
{
    Orbital *orbital = createStockedOrbital();
    Factory *factory = orbital->factory.get();
    ProductionCapture capture;
    Game::getCurrent()->addEventSink(&capture);

    SUBCASE("first build installs, second build is stored")
    {
        factory->queueItem(ItemType::SDM);
        runQueue(factory);
        CHECK(orbital->sdm_installed);
        CHECK(orbital->stores.items[ItemType::SDM] == 0);

        factory->queueItem(ItemType::SDM);
        runQueue(factory);
        CHECK(orbital->stores.items[ItemType::SDM] == 1);

        // completion is reported either way
        CHECK(capture.completed == std::vector<int>{ItemType::SDM, ItemType::SDM});
    }

    SUBCASE("AOC installs on the factory")
    {
        factory->queueItem(ItemType::AOC);
        runQueue(factory);
        CHECK(factory->aoc_installed);
        CHECK(orbital->stores.items[ItemType::AOC] == 0);
    }

    SUBCASE("ordinary production is unaffected")
    {
        factory->queueItem(ItemType::Supply_Pod);
        runQueue(factory);
        CHECK(orbital->stores.items[ItemType::Supply_Pod] == 1);
        CHECK_FALSE(orbital->sdm_installed);
        CHECK_FALSE(orbital->mtx_installed);
        CHECK_FALSE(factory->aoc_installed);
    }

    SUBCASE("repeating a feature installs once then stores")
    {
        factory->queueItem(ItemType::SDM);
        factory->repeatQueueItem(0, true);
        const int build_time = factory->queue[0].build_time;
        // the first tick starts the item, then build_time ticks complete it
        for (int tick = 0; tick < 2 * (build_time + 1); ++tick)
        {
            factory->update();
        }
        CHECK(orbital->sdm_installed);
        CHECK(orbital->stores.items[ItemType::SDM] == 1);
    }

    Game::getCurrent()->removeEventSink(&capture);
}

TEST_CASE("SaveGame round-trips installed facility features")
{
    Orbital *orbital = createStockedOrbital();
    Factory *factory = orbital->factory.get();
    const int orbitalId = orbital->id;

    factory->queueItem(ItemType::SDM);
    factory->queueItem(ItemType::AOC);
    factory->queueItem(ItemType::MTX);
    runQueue(factory);
    REQUIRE(orbital->sdm_installed);
    REQUIRE(factory->aoc_installed);
    REQUIRE(orbital->mtx_installed);

    SaveGame saver;
    REQUIRE(saver.save(FACTORY_SAVE_PATH) == 0);

    Game *loaded = Game::createCurrent();
    Loader loader(FACTORY_SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded->initialise(&loader));

    Orbital *lOrbital = static_cast<Orbital *>(loaded->locationByID(orbitalId));
    REQUIRE(lOrbital != nullptr);
    REQUIRE(lOrbital->factory != nullptr);
    CHECK(lOrbital->sdm_installed);
    CHECK(lOrbital->mtx_installed);
    CHECK(lOrbital->factory->aoc_installed);
    CHECK(lOrbital->stores.items[ItemType::SDM] == 0);
    CHECK(lOrbital->stores.items[ItemType::AOC] == 0);
    CHECK(lOrbital->stores.items[ItemType::MTX] == 0);

    remove(FACTORY_SAVE_PATH);
}
