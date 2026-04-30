#include "pages/bay_view.h"
#include "state/game.h"
#include "pages/overlay.h"

const char *bayTypeName[]{
    "Shuttle",
    "Space"};

// part elements

// non-spine sections are 96 wide
// cockpit spine 32
// pod spine 224
// drive spine 80

Rectangle cockpit_spine{1056, 72, 32, 48};
Rectangle spine{1096, 72, 224, 48};
Rectangle drive_spine{1328, 72, 80, 48};
Rectangle cockpit{1264, 304, 96, 80};
Rectangle drive{1160, 304, 96, 80};
Rectangle gantry_lower{1056, 248, 96, 32};
Rectangle gantry_upper{1056, 176, 96, 32};
Rectangle tool_pod{1160, 128, 96, 80};
Rectangle supply_pod{1472, 128, 96, 80};
Rectangle cryo_pod{1368, 128, 96, 80};

Rectangle shuttle_chassis{783, 389, 432, 80};
Rectangle ios_chassis{786, 508, 880, 80};

// render output and interaction areas
// full width = 320 + 224 * 4 = 1214
Rectangle cockpit_dest{1214 - 128 * 4, 300, 96 * 4, 80 * 4};
Rectangle cockpit_spine_dest{1214 - 32 * 4, 300 + 24 * 4, 32 * 4, 48 * 4};
Rectangle drive_spine_dest{640 - 80 * 4, 300 + 24 * 4, 80 * 4, 48 * 4};
Rectangle pod_spine_dest{320, 300 + 24 * 4, 224 * 4, 48 * 4};
Rectangle main_section_dest{640, 300, 96 * 4, 80 * 4};

void BayView::activate(ViewState &viewState)
{
    auto game{Game::getCurrent()};
    auto l{viewState.getCurrentLocation()};

    Facility *f{nullptr};
    switch (sublocationType)
    {
    case SublocationType::SLOC_ORBIT:
        f = game->orbitalAt(l);
        break;
    case SublocationType::SLOC_SURFACE:
        f = game->resourceFacilityAt(l);
        break;
    default:
        break;
    }

    if (f)
    {
        facility = f;
        std::snprintf(title, sizeof title, "%s %s Bay", SublocationTypeName[sublocationType], bayTypeName[type]);

        int dlg_width = GetScreenWidth() - 600;
        int dlg_height = GetScreenHeight() - 400;
        Rectangle dlg_dest{(float)dlg_width * 0.5f, (float)dlg_height * 0.5f, (float)dlg_width, (float)dlg_height};
        resourceList.activate(game, &facility->stores, dlg_dest);
        itemList.activate(game, &facility->stores, dlg_dest);

        craft = getCraft(); // initial docked craft on page activation
    }
}

void BayView::loadSupplyPod(Pod *pod)
{
    // set resource list selection to current pod content, if any

    resourceList.itemActive = -1;
    if (pod->amount > 0)
    {
        for (int i = 0; i < ResourceType::Count; i++)
        {
            if (pod->contentType == resourceList.activeResourceIDs[i])
            {
                resourceList.itemActive = i;
                resourceList.itemFocused = i;
                break;
            }
        }
    }
    resourceList.visible = true;
}

void BayView::loadToolPod(Pod *pod)
{
    // set item list selection to current pod content, if any

    itemList.itemActive = -1;
    if (pod->amount > 0)
    {
        for (int i = 0; i < itemList.currentCount; i++)
        {
            if (pod->contentType == itemList.activeItemsIDs[i])
            {
                itemList.itemActive = i;
                itemList.itemFocused = i;
                break;
            }
        }
    }
    itemList.visible = true;
}

void BayView::input()
{
    if (IsKeyPressed(KEY_A) && section > 0)
    {
        --section;
    }
    if (IsKeyPressed(KEY_D) && (section < driveSection))
    {
        ++section;
    }
    if ((section > 0) && (section < driveSection))
    {
        if (craft)
        {
            int podIndex = section - 1;
            if (IsKeyPressed(KEY_ONE))
            {
                // set to tool
                Game::getCurrent()->setPodType(craft, podIndex, PT_TOOL, facility);
            }
            if (IsKeyPressed(KEY_TWO))
            {
                // set to supply
                Game::getCurrent()->setPodType(craft, podIndex, PT_SUPPLY, facility);
            }
            if (IsKeyPressed(KEY_THREE))
            {
                // set to cryo
                Game::getCurrent()->setPodType(craft, podIndex, PT_CRYO, facility);
            }

            bool inspectPod = false;

            if (IsKeyPressed(KEY_S))
            {
                inspectPod = true;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                Vector2 mousePos = GetMousePosition();
                if (CheckCollisionPointRec(mousePos, main_section_dest))
                {
                    inspectPod = true;
                }
            }

            if (inspectPod)
            {
                // if supply pod, choose resource
                if (craft->pods[podIndex].type == PT_SUPPLY)
                {
                    loadSupplyPod(&craft->pods[podIndex]);
                }
                else if (craft->pods[podIndex].type == PT_TOOL)
                {
                    loadToolPod(&craft->pods[podIndex]);
                }
            }
        }
    }

    if (resourceList.visible)
    {
        // if RMB, close resource list
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            resourceList.visible = false;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            // clicked on a resource, get the ID and set it for the current pod
            int resourceID = resourceList.getFocusResourceID();
            if (resourceID >= 0)
            {
                auto &pod = craft->pods[section - 1];
                Game::getCurrent()->setSupplyPodContent(&pod, &facility->stores, resourceID, MAX_SUPPLY_POD_AMOUNT);
            }
        }
    }

    if (itemList.visible)
    {
        // if RMB, close item list
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            itemList.visible = false;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            // clicked on an item, get the ID and set it for the current pod
            int item_id = itemList.getFocusItemID();
            itemList.game->setToolPodContent(&craft->pods[section - 1], &facility->stores, item_id);
        }
    }

    // if click in background space, go to cockpit
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        Vector2 mousePos = GetMousePosition();
        if (CheckCollisionPointRec(mousePos, cockpit_dest))
        {
            // set viewState to cockpit view for this craft
            auto &pm{PageManager::getInstance()};
            if (craft->type == CT_SHUTTLE)
            {
                pm.viewState.setCurrentCraft(craft);
                pm.switchToPage(PAGE_SHUTTLE);
            }
            else if (craft->type == CT_IOS)
            {
                pm.viewState.setCurrentCraft(craft);
                pm.switchToPage(PAGE_COCKPIT);
            }
        }
    }
};

Craft *BayView::getCraft()
{
    if (type == BT_SHUTTLE)
    {
        return getShuttle();
    }
    else if (type == BT_SPACE)
    {
        return getSpacecraft();
    }
    return nullptr;
}

Craft *BayView::getSpacecraft()
{
    if (type == BT_SPACE)
    {
        // for now we only have one type of spacecraft, the IOS, so return that if it exists here
        for (auto &ios : Game::getCurrent()->allIOS())
        {
            if ((ios->location == facility->location) && (ios->state == CS_ORBIT_DOCKED))
            {
                return ios.get();
            }
        }
    }
    return nullptr;
}

// locate shuttle by finding if there is a shuttle for this location, at this faciliity.
// Predicated on one shuttle per location.
Shuttle *BayView::getShuttle()
{
    if (type == BT_SHUTTLE)
    {
        auto shuttle = facility->location->shuttle.get();
        if (shuttle)
        {
            // has a shuttle. Is it docked here?
            bool docked = false;
            if ((facility->sublocation == SLOC_ORBIT) && (shuttle->state == CS_ORBIT_DOCKED))
            {
                docked = true;
            }
            else if ((facility->sublocation == SLOC_SURFACE) && (shuttle->state == CS_SURFACE_DOCKED))
            {
                docked = true;
            }

            if (docked)
            {
                return shuttle;
            }
        }
    }
    return nullptr;
}

void BayView::render()
{
    BasePage::render();

    if (!facility)
    {
        return;
    }

    if (craft)
    {
        renderCraft();

        if (resourceList.visible)
        {
            resourceList.render();
        }
        if (itemList.visible)
        {
            itemList.render();
        }
    }

    // new craft button
    auto &overlay = Overlay::getInstance();
    Rectangle buttonDest{400, 900, 120, 50};
    const char *toolTip = (type == BT_SHUTTLE) ? "Construct a new shuttle from this bay" : "Construct a new IOS in this bay";

    bool enabled = (craft == nullptr) && (type == BT_SHUTTLE) ? Game::getCurrent()->canCommissionShuttle(facility) : Game::getCurrent()->canCommissionIOS(facility);

    if (!enabled)
    {
        toolTip = (type == BT_SHUTTLE) ? "A shuttle requires a chassis item in the facility stores" : "An IOS requires a chassis item in the facility stores";
        GuiDisable();
    }

    const char *buttonText = (type == BT_SHUTTLE) ? "New Shuttle" : "New IOS";

    if (overlay.renderButton(buttonDest, buttonText, toolTip, GRAY))
    {
        if (type == BT_SHUTTLE)
        {
            craft = Game::getCurrent()->commissionShuttle(facility);
        }
        else if (type == BT_SPACE)
        {
            craft = Game::getCurrent()->commissionIOS(facility);
        }
    }
    GuiEnable();
}

void BayView::update(const float delta)
{
    // update craft state - e.g. if still docked or not

    craft = getCraft();

    // animation
}

void BayView::renderCraft()
{
    if (!craft)
    {
        return;
    }
    driveSection = craft->max_pods + 1; // drive section after last pod
    if (section == 0)
    {

        DrawTexturePro(*partsTexture, cockpit, cockpit_dest, Vector2{0, 0}, 0.0f, WHITE);
        DrawTexturePro(*partsTexture, cockpit_spine, cockpit_spine_dest, Vector2{0, 0}, 0.0f, WHITE);
    }
    else if (section == driveSection)
    {
        renderDrive();
    }
    else
    {
        renderPod(&craft->pods[section - 1]);
    }
}

void BayView::renderPod(Pod *pod)
{
    DrawTexturePro(*partsTexture, spine, pod_spine_dest, Vector2{0, 0}, 0.0f, WHITE);
    switch (pod->type)
    {
    case PT_TOOL:
        DrawTexturePro(*partsTexture, tool_pod, main_section_dest, Vector2{0, 0}, 0.0f, WHITE);
        // text display of pod content item name and amount, centered below pod
        if (pod->amount > 0)
        {
            char buffer[64];
            std::snprintf(buffer, sizeof buffer, "%s: %d", Game::getCurrent()->items[pod->contentType].name, pod->amount);
            int textWidth = MeasureText(buffer, 20);
            DrawText(buffer, (int)(main_section_dest.x + main_section_dest.width / 2 - textWidth / 2), (int)(main_section_dest.y + main_section_dest.height + 10), 20, WHITE);
        }
        break;
    case PT_SUPPLY:
        DrawTexturePro(*partsTexture, supply_pod, main_section_dest, Vector2{0, 0}, 0.0f, WHITE);
        if (pod->amount > 0)
        {
            // text display of resource name and amount, centered below pod
            char buffer[64];
            std::snprintf(buffer, sizeof buffer, "%s: %d", ResourceName[pod->contentType], pod->amount);
            int textWidth = MeasureText(buffer, 20);
            DrawText(buffer, (int)(main_section_dest.x + main_section_dest.width / 2 - textWidth / 2), (int)(main_section_dest.y + main_section_dest.height + 10), 20, WHITE);
        }
        break;
    case PT_CRYO:
        DrawTexturePro(*partsTexture, cryo_pod, main_section_dest, Vector2{0, 0}, 0.0f, WHITE);
        break;
    default:
        break;
    }
}

Rectangle driveSpineDest{};

void BayView::renderDrive()
{
    DrawTexturePro(*partsTexture, drive_spine, drive_spine_dest, Vector2{0, 0}, 0.0f, WHITE);
    if (craft->drive)
    {
        DrawTexturePro(*partsTexture, drive, main_section_dest, Vector2{0, 0}, 0.0f, WHITE);
    }
}