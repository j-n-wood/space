#include "doctest.h"
#include "../include/loaders/loader.h"
#include "../include/loaders/save_game.h"
#include "../include/state/game.h"
#include "../include/state/event.h"
#include "../include/state/event_sink.h"
#include "../include/state/orbital.h"
#include <sqlite3.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static const char *EVENTS_DB_PATH = "./resources/initial.db";
static const char *EVENTS_SAVE_PATH = "./test_events_save.db";

// initial.db: EVENT_ORBITAL_FACTORY_COMPLETED ("Reach orbit") unlocks the IOS research topics 7..13.
static const std::vector<int> REACH_ORBIT_TOPICS{7, 8, 9, 10, 11, 12, 13};

// Captures log lines raised by the game.
class LogCapture : public EventSink
{
public:
    std::vector<std::string> logs;
    void addLog(const char *log) override { logs.emplace_back(log); }
};

// Minimal in-memory schema for exercising Loader::loadEvents in isolation.
static void createEventTables(Loader &loader, const char *rows)
{
    std::string sql =
        "CREATE TABLE events ( id INTEGER PRIMARY KEY, name TEXT, log_message TEXT, email_message TEXT, completed INT, raise_at FLOAT );"
        "CREATE TABLE event_unlock_research_topics ( event_id INT, topic_id INT );";
    sql += rows;
    REQUIRE(sqlite3_exec(loader.db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);
}

static void addResearchTopics(Game &game, int count)
{
    game.researchTopics.resize(count);
    for (int i = 0; i < count; ++i)
    {
        game.researchTopics[i].id = i;
    }
}

static Orbital *createPlayerOrbitalAtEarth(Game *game, int faction_id = 0)
{
    Location *earth = game->locationByID(4);
    REQUIRE(earth != nullptr);
    Orbital *orbital = game->orbitalAt(earth);
    if (!orbital)
    {
        orbital = game->createOrbital(earth);
    }
    REQUIRE(orbital != nullptr);
    orbital->faction_id = faction_id;
    orbital->operational = true;
    return orbital;
}

TEST_CASE("Game::initialise loads events from initial.db")
{
    Game *game = Game::createCurrent();
    Loader loader(EVENTS_DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));

    REQUIRE_FALSE(game->events.empty());

    Event *reachOrbit = game->eventByID(EVENT_ORBITAL_FACTORY_COMPLETED);
    REQUIRE(reachOrbit != nullptr);
    CHECK(reachOrbit->id == EVENT_ORBITAL_FACTORY_COMPLETED);
    CHECK(std::strcmp(reachOrbit->name, "Reach orbit") == 0);
    CHECK_FALSE(reachOrbit->completed);
    CHECK(reachOrbit->unlocksTopics == REACH_ORBIT_TOPICS);
}

TEST_CASE("Loader::loadEvents tolerates NULL text columns")
{
    Game game;
    addResearchTopics(game, 4);
    Loader loader(":memory:");
    REQUIRE(loader.isValid());
    loader.setGame(&game);
    createEventTables(loader, "INSERT INTO events VALUES (1, 'Null fields', NULL, NULL, 0, NULL);");

    REQUIRE(loader.loadEvents());

    Event *event = game.eventByID(1);
    REQUIRE(event != nullptr);
    CHECK(std::strcmp(event->name, "Null fields") == 0);
    CHECK(event->log_message[0] == '\0');
    CHECK(event->email_message[0] == '\0');
}

TEST_CASE("Loader::loadEvents truncates over-long text safely")
{
    Game game;
    addResearchTopics(game, 4);
    Loader loader(":memory:");
    REQUIRE(loader.isValid());
    loader.setGame(&game);
    std::string longName(100, 'x');
    createEventTables(loader, ("INSERT INTO events VALUES (1, '" + longName + "', '', '', 0, 0);").c_str());

    REQUIRE(loader.loadEvents());

    Event *event = game.eventByID(1);
    REQUIRE(event != nullptr);
    // must be null terminated within the fixed buffer
    CHECK(std::strlen(event->name) == sizeof(event->name) - 1);
}

TEST_CASE("Loader::loadEvents with gaps in the id sequence")
{
    // ids are not required to start at 0 or be contiguous; lookups must skip the holes
    Game game;
    addResearchTopics(game, 4);
    Loader loader(":memory:");
    REQUIRE(loader.isValid());
    loader.setGame(&game);
    createEventTables(loader,
                      "INSERT INTO events VALUES (1, 'One', '', '', 0, 0);"
                      "INSERT INTO events VALUES (3, 'Three', '', '', 0, 0);"
                      "INSERT INTO event_unlock_research_topics VALUES (3, 2);");

    REQUIRE(loader.loadEvents());

    REQUIRE(game.events.size() == 4);

    // holes are filled with do-nothing events
    for (int hole : {0, 2})
    {
        Event *filler = game.eventByID(hole);
        REQUIRE(filler != nullptr);
        CHECK(filler->name[0] == '\0');
        CHECK(filler->log_message[0] == '\0');
        CHECK(filler->unlocksTopics.empty());
        CHECK_FALSE(filler->completed);
    }

    REQUIRE(game.eventByID(3) != nullptr);
    CHECK(game.eventByID(3)->id == 3);
    CHECK(game.eventByID(3)->unlocksTopics == std::vector<int>{2});

    CHECK(game.completeEvent(3));
    CHECK(game.researchTopics[2].available);
}

TEST_CASE("Loader::loadEvents rejects an unlock for an unknown event")
{
    Game game;
    addResearchTopics(game, 4);
    Loader loader(":memory:");
    REQUIRE(loader.isValid());
    loader.setGame(&game);
    createEventTables(loader,
                      "INSERT INTO events VALUES (1, 'One', '', '', 0, 0);"
                      "INSERT INTO event_unlock_research_topics VALUES (99, 2);");

    CHECK_FALSE(loader.loadEvents());
}

TEST_CASE("Loader::loadEvents rejects an unlock for an unknown research topic")
{
    Game game;
    addResearchTopics(game, 4);
    Loader loader(":memory:");
    REQUIRE(loader.isValid());
    loader.setGame(&game);
    createEventTables(loader,
                      "INSERT INTO events VALUES (1, 'One', '', '', 0, 0);"
                      "INSERT INTO event_unlock_research_topics VALUES (1, 4);");

    CHECK_FALSE(loader.loadEvents());
}

TEST_CASE("Game::completeEvent")
{
    Game game;
    addResearchTopics(game, 4);
    game.events.resize(EVENT_MAX);
    game.events[EVENT_ORBITAL_FACTORY_COMPLETED] = Event(EVENT_ORBITAL_FACTORY_COMPLETED, "Test", "Something happened", "", false, 0.0);
    game.events[EVENT_ORBITAL_FACTORY_COMPLETED].unlocksTopics = {1, 3};
    Event &event = game.events[EVENT_ORBITAL_FACTORY_COMPLETED];

    LogCapture capture;
    game.addEventSink(&capture);

    SUBCASE("unlocks topics, logs, and marks completed")
    {
        CHECK(game.completeEvent(EVENT_ORBITAL_FACTORY_COMPLETED));
        CHECK(event.completed);
        CHECK_FALSE(game.researchTopics[0].available);
        CHECK(game.researchTopics[1].available);
        CHECK_FALSE(game.researchTopics[2].available);
        CHECK(game.researchTopics[3].available);
        REQUIRE(capture.logs.size() == 1);
        CHECK(capture.logs[0] == "Something happened");
    }

    SUBCASE("fires only once")
    {
        CHECK(game.completeEvent(EVENT_ORBITAL_FACTORY_COMPLETED));
        CHECK_FALSE(game.completeEvent(EVENT_ORBITAL_FACTORY_COMPLETED));
        CHECK(capture.logs.size() == 1);
    }

    SUBCASE("unknown id is refused")
    {
        CHECK_FALSE(game.completeEvent(EVENT_MAX));
        CHECK_FALSE(game.completeEvent(42));
        CHECK_FALSE(game.completeEvent(-1));
        CHECK(capture.logs.empty());
    }

    SUBCASE("do-nothing event changes nothing but its completed flag")
    {
        CHECK(game.completeEvent(EVENT_NONE));
        CHECK(game.events[EVENT_NONE].completed);
        for (auto &topic : game.researchTopics)
        {
            CHECK_FALSE(topic.available);
        }
        CHECK(capture.logs.empty());
    }

    SUBCASE("empty log message raises no log")
    {
        event.log_message[0] = '\0';
        CHECK(game.completeEvent(EVENT_ORBITAL_FACTORY_COMPLETED));
        CHECK(capture.logs.empty());
    }

    game.removeEventSink(&capture);
}

TEST_CASE("first operational player orbital completes the Reach orbit event")
{
    Game *game = Game::createCurrent();
    Loader loader(EVENTS_DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));
    REQUIRE(game->eventByID(EVENT_ORBITAL_FACTORY_COMPLETED) != nullptr);

    for (int topic : REACH_ORBIT_TOPICS)
    {
        REQUIRE_FALSE(game->researchTopics[topic].available);
    }

    SUBCASE("player orbital fires the event")
    {
        Orbital *orbital = createPlayerOrbitalAtEarth(game);
        game->raiseOrbitalConstructionEvent(orbital);

        CHECK(game->eventByID(EVENT_ORBITAL_FACTORY_COMPLETED)->completed);
        for (int topic : REACH_ORBIT_TOPICS)
        {
            CHECK(game->researchTopics[topic].available);
        }
    }

    SUBCASE("non-player orbital does not fire the event")
    {
        Orbital *orbital = createPlayerOrbitalAtEarth(game, 1);
        game->raiseOrbitalConstructionEvent(orbital);

        CHECK_FALSE(game->eventByID(EVENT_ORBITAL_FACTORY_COMPLETED)->completed);
        CHECK_FALSE(game->researchTopics[REACH_ORBIT_TOPICS[0]].available);
    }

    SUBCASE("orbital under construction does not fire the event")
    {
        Orbital *orbital = createPlayerOrbitalAtEarth(game);
        orbital->operational = false;
        game->raiseOrbitalConstructionEvent(orbital);

        CHECK_FALSE(game->eventByID(EVENT_ORBITAL_FACTORY_COMPLETED)->completed);
    }
}

TEST_CASE("SaveGame round-trips events")
{
    Game *game = Game::createCurrent();
    {
        Loader loader(EVENTS_DB_PATH);
        REQUIRE(loader.isValid());
        REQUIRE(game->initialise(&loader));
    }
    REQUIRE(game->eventByID(EVENT_ORBITAL_FACTORY_COMPLETED) != nullptr);
    REQUIRE(game->eventByID(EVENT_FACTION_HOSTILITY) != nullptr);
    REQUIRE(game->completeEvent(EVENT_ORBITAL_FACTORY_COMPLETED));

    SaveGame saver;
    REQUIRE(saver.save(EVENTS_SAVE_PATH) == 0);

    Game *loaded = Game::createCurrent();
    Loader loader(EVENTS_SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded->initialise(&loader));

    Event *reachOrbit = loaded->eventByID(EVENT_ORBITAL_FACTORY_COMPLETED);
    REQUIRE(reachOrbit != nullptr);
    CHECK(reachOrbit->completed);
    CHECK(std::strcmp(reachOrbit->name, "Reach orbit") == 0);
    CHECK(reachOrbit->unlocksTopics == REACH_ORBIT_TOPICS);

    Event *hostility = loaded->eventByID(EVENT_FACTION_HOSTILITY);
    REQUIRE(hostility != nullptr);
    CHECK_FALSE(hostility->completed);

    // a completed event must not fire again after load
    CHECK_FALSE(loaded->completeEvent(EVENT_ORBITAL_FACTORY_COMPLETED));

    remove(EVENTS_SAVE_PATH);
}

TEST_CASE("initial.db defines every named event")
{
    // hardcoded triggers index events by EventID, so each one must have a real row
    Game *game = Game::createCurrent();
    Loader loader(EVENTS_DB_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(game->initialise(&loader));

    REQUIRE(game->events.size() >= EVENT_MAX);
    for (int id = EVENT_NONE + 1; id < EVENT_MAX; ++id)
    {
        CAPTURE(id);
        CHECK(game->events[id].id == id);
        CHECK(game->events[id].name[0] != '\0');
    }
}

TEST_CASE("SaveGame round-trips events with gaps in the id sequence")
{
    Game *game = Game::createCurrent();
    {
        Loader loader(EVENTS_DB_PATH);
        REQUIRE(loader.isValid());
        REQUIRE(game->initialise(&loader));
    }
    // leave a hole below a new event
    int gap_id = static_cast<int>(game->events.size());
    int new_id = gap_id + 1;
    game->events.resize(new_id + 1);
    game->events[new_id] = Event(static_cast<EventID>(new_id), "Late event", "", "", false, 0.0);

    SaveGame saver;
    REQUIRE(saver.save(EVENTS_SAVE_PATH) == 0);

    Game *loaded = Game::createCurrent();
    Loader loader(EVENTS_SAVE_PATH);
    REQUIRE(loader.isValid());
    REQUIRE(loaded->initialise(&loader));

    REQUIRE(loaded->events.size() == game->events.size());
    CHECK(loaded->events[EVENT_ORBITAL_FACTORY_COMPLETED].unlocksTopics == REACH_ORBIT_TOPICS);
    CHECK(loaded->events[gap_id].name[0] == '\0');
    CHECK(std::strcmp(loaded->events[new_id].name, "Late event") == 0);
    CHECK(loaded->events[new_id].id == new_id);

    remove(EVENTS_SAVE_PATH);
}
