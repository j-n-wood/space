#include "pages/pages.h"

// TODO - want to self-register pages?

#include "pages/earth_city.h"
#include "pages/system_view.h"
#include "pages/resources.h"
#include "pages/stores_view.h"
#include "pages/factory_view.h"
#include "pages/shuttle_view.h"
#include "pages/bay_view.h"
#include "pages/research.h"
#include "pages/master_control.h"
#include "pages/orbital_view.h"
#include "pages/training_view.h"
#include "assets/ui_elements.h"

PageManager::PageManager() : currentPage(nullptr), desiredPage(PAGE_NONE), modal(false)
{
    // initialize page resources
    for (int i = 0; i < PAGE_COUNT; i++)
    {
        pages[i] = nullptr;
    }

    pages[PAGE_EARTH_CITY] = new EarthCityView();
    pages[PAGE_SYSTEM_VIEW] = new SystemView();
    pages[PAGE_SURFACE_RESOURCES] = new Resources();
    pages[PAGE_SURFACE_STORES] = new StoresView(LOCATION_TYPE_SURFACE);
    pages[PAGE_ORBIT_STORES] = new StoresView(LOCATION_TYPE_ORBIT);
    pages[PAGE_PRODUCTION] = new FactoryView(LOCATION_TYPE_ORBIT);
    pages[PAGE_SURFACE_PRODUCTION] = new FactoryView(LOCATION_TYPE_SURFACE);
    pages[PAGE_SHUTTLE] = new ShuttleView();
    pages[PAGE_COCKPIT] = new ShuttleView();
    pages[PAGE_SURFACE_SHUTTLE_BAY] = new BayView(LOCATION_TYPE_SURFACE, BT_SHUTTLE); // TODO subclass?
    pages[PAGE_ORBIT_SHUTTLE_BAY] = new BayView(LOCATION_TYPE_ORBIT, BT_SHUTTLE);
    pages[PAGE_ORBIT_SPACE_BAY] = new BayView(LOCATION_TYPE_ORBIT, BT_SPACE);
    pages[PAGE_EARTH_RESEARCH] = new ResearchView();
    pages[PAGE_MASTER_CONTROL] = new MasterControlView();
    pages[PAGE_ORBITAL] = new OrbitalView();
    pages[PAGE_EARTH_TRAINING] = new TrainingView();
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

bool PageManager::switchToPage(Page newPage)
{
    // logic to switch to the specified page, for example by creating a new page instance and setting it as the current page
    // this is just a placeholder, actual implementation would depend on how you manage page instances and rendering

    if (modal)
    {
        return false; // cannot switch pages while a modal page is active
    }

    BasePage *newPageInstance = pages[newPage];

    if (newPageInstance == nullptr)
    {

        return false;
    }

    desiredPage = newPage;
    return true;
}

void PageManager::render()
{
    ControlLockToggle lock(modal); // lock controls during rendering if there is a modal
    if (currentPage)
    {
        currentPage->render();
    }
}

void PageManager::update()
{
    if (desiredPage != PAGE_NONE)
    {
        BasePage *newPageInstance = pages[desiredPage];
        if ((newPageInstance != nullptr) && (newPageInstance != currentPage))
        {
            if (currentPage)
            {
                currentPage->deactivate(); // call deactivate on the old page to clean up any state
            }
            currentPage = newPageInstance;
            currentPage->activate(viewState); // call activate on the new page to set it up
        }
        desiredPage = PAGE_NONE;
    }
}

BasePage *PageManager::reactivateCurrentPage()
{
    if (currentPage)
    {
        currentPage->deactivate();        // call deactivate to clean up any existing state
        currentPage->activate(viewState); // re-activate current page to update any state based on new game state
    }
    return currentPage;
}

// note this do not check modal state - need some worked examples
void PageManager::onFacilityDestroyed(Facility *f)
{
    // if current facility was destroyed, jump to system view
    if (currentPage && viewState.clearFacilityFocus(f))
    {
        switchToPage(PAGE_SYSTEM_VIEW);
    }
}

void PageManager::onCraftDestroyed(Craft *c)
{
    if (currentPage && viewState.clearCraftFocus(c))
    {
        switchToPage(PAGE_MASTER_CONTROL);
    }
}

void PageManager::onCraftUnderAttack(Craft *c)
{
    viewState.setNotifyPlayer(true); // notify player that a craft is under attack
}

void PageManager::onFactionInteraction(Faction *faction, FactionInteraction interaction, Craft *craft)
{
    // show faction interaction modal or notification
    // may shift focus to craft that initiated the interaction
    // as change page is a delayed action, so must be opening a modal
    if (faction && interaction != FactionInteraction::Unset && craft)
    {
        // open faction interaction modal or notification
        viewState.setCraftFocus(craft);
        switchToPage(PAGE_COCKPIT);
        // trigger needs to be set s.t. page shows comms dialog on open
        // currently detected by the page on activation
    }
}