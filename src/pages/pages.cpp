#include "pages/pages.h"

// TODO - want to self-register pages?

#include "pages/earth_city.h"
#include "pages/system_view.h"
#include "pages/resources.h"
#include "pages/stores_view.h"
#include "pages/factory_view.h"

PageManager::PageManager() : currentPage(nullptr)
{
    // initialize page resources
    for (int i = 0; i < PAGE_COUNT; i++)
    {
        pages[i] = nullptr;
    }

    pages[PAGE_EARTH_CITY] = new EarthCity();
    pages[PAGE_SYSTEM_VIEW] = new SystemView();
    pages[PAGE_SURFACE_RESOURCES] = new Resources();
    pages[PAGE_SURFACE_STORES] = new StoresView(SublocationType::SLOC_SURFACE);
    pages[PAGE_ORBIT_STORES] = new StoresView(SublocationType::SLOC_ORBIT);
    pages[PAGE_PRODUCTION] = new FactoryView();
}

PageManager::~PageManager()
{
    // clean up page resources here
    for (int i = 0; i < PAGE_COUNT; i++)
    {
        delete pages[i];
        pages[i] = nullptr;
    }
}

BasePage *PageManager::switchToPage(Page newPage)
{
    // logic to switch to the specified page, for example by creating a new page instance and setting it as the current page
    // this is just a placeholder, actual implementation would depend on how you manage page instances and rendering

    BasePage *newPageInstance = nullptr;

    switch (newPage)
    {
    case PAGE_MAIN_MENU:
        // switch to main menu page
        break;
    case PAGE_EARTH_CITY:
        // switch to earth city page
        newPageInstance = pages[PAGE_EARTH_CITY];
        break;
    case PAGE_EARTH_TRAINING:
        // switch to earth training page
        break;
    case PAGE_SURFACE_RESOURCES:
        newPageInstance = pages[PAGE_SURFACE_RESOURCES];
        break;
    case PAGE_SYSTEM_VIEW:
        // switch to system view page
        newPageInstance = pages[PAGE_SYSTEM_VIEW];
        break;
    case PAGE_SURFACE_STORES:
        newPageInstance = pages[PAGE_SURFACE_STORES];
        break;
    case PAGE_ORBIT_STORES:
        newPageInstance = pages[PAGE_ORBIT_STORES];
        break;
    case PAGE_PRODUCTION:
        newPageInstance = pages[PAGE_PRODUCTION];
        break;
    case PAGE_SETTINGS:
        // switch to settings page
        break;
    default:
        // handle invalid page if needed
        break;
    }

    if (newPageInstance != nullptr && newPageInstance != currentPage)
    {
        currentPage = newPageInstance;
        currentPage->activate(); // call activate on the new page to set it up
    }
    return currentPage;
}