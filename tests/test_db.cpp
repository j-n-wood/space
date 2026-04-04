#include "unity.h"
#include "../include/loaders/loader.h"
#include "../include/loaders/load_system.h"
#include "../include/system.h"
#include <sqlite3.h>

// This runs before every test
void setUp(void) {}

// This runs after every test
void tearDown(void) {}

void test_DatabaseClose_ShouldNotCrash(void) {
    // 1. Initialize your custom struct
    Loader* loader = createLoader(":memory:");
    TEST_ASSERT_NOT_NULL(loader);

    // 3. The Close - If a double-free occurs, the test runner will abort here
    destroyLoader(loader);
}

void test_DatabaseReadClose_ShouldNotCrash(void) {
    // 1. Initialize your custom struct
    Loader* loader = createLoader("./resources/initial.db");
    TEST_ASSERT_NOT_NULL(loader);

    // create a system and load from loader
    SystemPtr system = createSystem(false);
    TEST_ASSERT_NOT_NULL(system);
    TEST_ASSERT_TRUE(loadSystem(loader, 1, system.get()));

    // system shall have 8 planets based on our initial.db
    TEST_ASSERT_EQUAL_INT(8, system->numPlanets);

    // 3. The Close - If a double-free occurs, the test runner will abort here
    destroyLoader(loader);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_DatabaseClose_ShouldNotCrash);
    RUN_TEST(test_DatabaseReadClose_ShouldNotCrash);
    return UNITY_END();
}