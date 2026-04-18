#include "pages/pages.h"

// TODO - want to self-register pages?

#include "pages/earth_city.h"
#include "pages/system_view.h"
#include "pages/resources.h"
#include "pages/stores_view.h"
#include "pages/factory_view.h"
#include "pages/shuttle_view.h"
#include "pages/bay_view.h"

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
    pages[PAGE_PRODUCTION] = new FactoryView(SublocationType::SLOC_ORBIT);
    pages[PAGE_SURFACE_PRODUCTION] = new FactoryView(SublocationType::SLOC_SURFACE);
    pages[PAGE_SHUTTLE] = new ShuttleView();
    pages[PAGE_SURFACE_SHUTTLE_BAY] = new BayView(SublocationType::SLOC_SURFACE, BT_SHUTTLE); // TODO subclass?
    pages[PAGE_ORBIT_SHUTTLE_BAY] = new BayView(SublocationType::SLOC_ORBIT, BT_SHUTTLE);
    pages[PAGE_ORBIT_SPACE_BAY] = new BayView(SublocationType::SLOC_ORBIT, BT_SPACE);
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

    BasePage *newPageInstance = pages[newPage];

    if (newPageInstance != nullptr && newPageInstance != currentPage)
    {
        currentPage = newPageInstance;
        currentPage->activate(viewState); // call activate on the new page to set it up
    }
    return currentPage;
}