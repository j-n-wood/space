#pragma once

#include "base_page.h"

typedef enum {
    PAGE_MAIN_MENU,
    PAGE_EARTH_CITY,
    PAGE_EARTH_TRAINING,
    PAGE_BODY_RESOURCES,
    PAGE_SYSTEM_VIEW,
    PAGE_BODY_VIEW,
    PAGE_SETTINGS,
    PAGE_COUNT
} Page;

// singleton page manager, allow switching pages
// owns page resources
// note data objects that a page point to depend on other state

class PageManager {
    // array of page implementations
    BasePage* pages[PAGE_COUNT];
    BasePage* currentPage; // currently active page
public:
    PageManager();
    ~PageManager();

    BasePage* switchToPage(Page newPage);
    inline BasePage* getCurrentPage() const { return currentPage; }

    // singleton access
    static PageManager& getInstance() {
        static PageManager instance; // guaranteed to be destroyed, instantiated on first use
        return instance;
    }    
};