#pragma once

#include "base_page.h"

typedef enum {
    PAGE_NONE,
    PAGE_MAIN_MENU,
    PAGE_EARTH_CITY,
    PAGE_EARTH_TRAINING,
    PAGE_EARTH_RESEARCH,
    PAGE_PRODUCTION,
    PAGE_ORBIT_STORES,
    PAGE_ORBIT_SHUTTLE_BAY,
    PAGE_ORBIT_SPACE_BAY,
    PAGE_SYSTEM_VIEW,
    PAGE_SURFACE_SHUTTLE_BAY,
    PAGE_SURFACE_RESOURCES,
    PAGE_SURFACE_STORES,
    PAGE_SHUTTLE,
    PAGE_COCKPIT,    
    PAGE_SELF_DESTRUCT,    
    PAGE_SETTINGS,
    PAGE_COUNT
} Page;

// singleton page manager, allow switching pages
// owns page resources
// note data objects that a page point to depend on other state

class BasePage; // forward declaration

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