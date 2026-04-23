#include "pages/bay_view.h"
#include "state/game.h"

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
        resourceList.setStores(&facility->stores);

        int dlg_width = GetScreenWidth() - 400;
        int dlg_height = GetScreenHeight() - 300;
        resourceList.dest = (Rectangle){200, 150, (float)dlg_width, (float)dlg_height};
    }
};

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
        if (auto craft = getShuttle())
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
            if (IsKeyPressed(KEY_S))
            {
                // if supply pod, choose resource
                if (craft->pods[podIndex].type == PT_SUPPLY)
                {
                    // set resource list selection to current pod content, if any
                    resourceList.itemActive = -1;
                    if (craft->pods[podIndex].amount > 0)
                    {
                        for (int i = 0; i < ResourceType::Count; i++)
                        {
                            if (craft->pods[podIndex].contentType == resourceList.activeResourceIDs[i])
                            {
                                resourceList.itemActive = i;
                                resourceList.itemFocused = i;
                                break;
                            }
                        }
                    }
                    resourceList.visible = true;
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
            Shuttle *shuttle = getShuttle();
            // clicked on a resource, get the ID and set it for the current pod
            int resourceID = resourceList.getFocusResourceID();
            if (resourceID >= 0)
            {
                int podIndex = section - 1;
                // remove existing content if any
                if (shuttle->pods[podIndex].amount > 0)
                {
                    facility->stores.resources[shuttle->pods[podIndex].contentType] += shuttle->pods[podIndex].amount;
                }
                // set new content
                shuttle->pods[podIndex].contentType = resourceID;
                // set to available amount in stores, or max pod capacity, whichever is lower. max is 250
                shuttle->pods[podIndex].amount = std::min(facility->stores.resources[resourceID], 250);
                // remove from stores
                facility->stores.resources[resourceID] -= shuttle->pods[podIndex].amount;
            }
        }
    }
};

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

    if (type == BT_SHUTTLE)
    {
        auto shuttle = getShuttle();
        if (shuttle)
        {
            renderShuttle(shuttle);

            if (resourceList.visible)
            {
                resourceList.render();
            }
        }
    }
};

// full width = 320 + 224 * 4 = 1214
Rectangle cockpit_dest{1214 - 128 * 4, 300, 96 * 4, 80 * 4};
Rectangle cockpit_spine_dest{1214 - 32 * 4, 300 + 24 * 4, 32 * 4, 48 * 4};
Rectangle drive_spine_dest{640 - 80 * 4, 300 + 24 * 4, 80 * 4, 48 * 4};
Rectangle pod_spine_dest{320, 300 + 24 * 4, 224 * 4, 48 * 4};
Rectangle main_section_dest{640, 300, 96 * 4, 80 * 4};

void BayView::renderShuttle(Shuttle *shuttle)
{
    driveSection = 2;
    switch (section)
    {
    case 0:
        DrawTexturePro(*partsTexture, cockpit, cockpit_dest, Vector2{0, 0}, 0.0f, WHITE);
        DrawTexturePro(*partsTexture, cockpit_spine, cockpit_spine_dest, Vector2{0, 0}, 0.0f, WHITE);
        break;
    case 1:
        renderPod(&shuttle->pods[0]);
        break;
    case 2:
        renderDrive(shuttle);
        break;
    }
}

void BayView::renderPod(Pod *pod)
{
    DrawTexturePro(*partsTexture, spine, pod_spine_dest, Vector2{0, 0}, 0.0f, WHITE);
    switch (pod->type)
    {
    case PT_TOOL:
        DrawTexturePro(*partsTexture, tool_pod, main_section_dest, Vector2{0, 0}, 0.0f, WHITE);
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

void BayView::renderDrive(Shuttle *shuttle)
{
    DrawTexturePro(*partsTexture, drive_spine, drive_spine_dest, Vector2{0, 0}, 0.0f, WHITE);
    if (shuttle->drive)
    {
        DrawTexturePro(*partsTexture, drive, main_section_dest, Vector2{0, 0}, 0.0f, WHITE);
    }
}