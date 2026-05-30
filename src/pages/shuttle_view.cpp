#include "pages/shuttle_view.h"
#include "pages/overlay.h"
#include "assets/ui_elements.h"
#include "state/game.h"
#include "state/location.h"
#include "state/autopilot.h"

// original images 208 x 120 -> 832 x 480
Rectangle viewportDest = {300, 200, 832, 480};

// source images for states
/*
typedef enum
{
    CS_SURFACE, // surface no dock
    CS_SURFACE_DOCKED,
    CS_SURFACE_DOCK_WORK, // transient state while docked and working
    CS_SURFACE_WORK,
    CS_SURFACE_LAUNCH, // transient state leaving dock
    CS_ASCENDING,
    CS_ORBIT,
    CS_ORBIT_DOCKING, // transient state entering dock
    CS_ORBIT_DOCKED,
    CS_ORBIT_DOCK_WORK, // transient state while docked and working
    CS_ORBIT_WORK,
    CS_ORBIT_LAUNCH, // transient state leaving dock
    CS_DESCENDING,
    CS_TRANSIT, // IP or IS transit - refine with type and speed
    CS_COUNT
} CraftState;
*/
// this won't do - image depends on location properties e.g. station presence (could overlay)
// or location type (star, planet, etc)
Rectangle viewportImages[CS_COUNT] = {
    {1368, 280, 208, 120}, // docked
    {1368, 280, 208, 120}, // docked
    {1368, 280, 208, 120}, // docked
    {1368, 280, 208, 120}, // docked
    {1368, 280, 208, 120}, // docked
    {1152, 280, 208, 120}, // storm doors
    {1368, 152, 208, 120}, // orbit no station
    {1368, 152, 208, 120}, // orbit no station
    {1368, 280, 208, 120}, // docked
    {1368, 280, 208, 120}, // docked
    {1368, 152, 208, 120}, // orbit no station
    {1152, 280, 208, 120}, // storm doors
    {1368, 152, 208, 120}, // orbit no station
    {1152, 408, 208, 120}, // transit
};

Rectangle pod_icon_coordinates[6] = {
    {810, 910, 96, 64},
    {920, 850, 96, 64},
    {1040, 800, 96, 64},
    {950, 940, 96, 64},
    {1090, 880, 96, 64},
    {1150, 940, 96, 64},
};

// if viewstate is set to a craft, show cockpit for that
// if not, look for shuttle at location
void ShuttleView::activate(ViewState &viewState)
{
    if ((craft = viewState.getCurrentCraft()) != nullptr)
    {
        location = craft->location;
    }
    else if (auto l = viewState.getCurrentLocation())
    {
        location = l;
        craft = location->shuttle.get();
    }

    if (craft)
    {
        if (craft->type == CT_SHUTTLE)
        {
            std::snprintf(title, sizeof title, "Shuttle");
        }
        else
        {
            std::snprintf(title, sizeof title, "%s", craft->name);
        }

        autopilotView = std::make_unique<AutopilotView>(craft->autopilot.get(), craft);
    }
    else
    {
        std::snprintf(title, sizeof title, "No Shuttle");
    }

    Game::getCurrent()->addEventSink(this);
}

void ShuttleView::deactivate()
{
    Game::getCurrent()->removeEventSink(this);
}

void destinationSelected(void *state, Location *loc)
{
    ShuttleView *shuttleView = static_cast<ShuttleView *>(state);
    if (shuttleView->craft && loc)
    {
        Craft *craft = shuttleView->craft;
        craft->setDestination(craft->destination_index, loc);
        craft->destinations[craft->destination_index].docked = (craft->autopilot->state >= AS_ON) && (Game::getCurrent()->orbitalAt(loc) != nullptr); // if autopilot on and destination has an orbital station, assume want to dock
        craft->destinations[craft->destination_index].sublocation = SLOC_ORBIT;                                                                       // for now assume always orbit, could be surface or city in future

        shuttleView->destinationPicker->visible = false;
    }
}

void destinationSelectCancelled(void *state)
{
    // no action needed, just close the picker
    ShuttleView *shuttleView = static_cast<ShuttleView *>(state);
    if (shuttleView->destinationPicker)
    {
        shuttleView->destinationPicker->visible = false;
    }
}

void ShuttleView::input()
{
    if (IsKeyPressed(KEY_D))
    {
        // dock/undock
        if (craft->state == CS_ORBIT_DOCKED)
        {
            craft->state = CS_ORBIT_LAUNCH;
            craft->state_timer = CSTD_LAUNCH;
        }
        else if (craft->state == CS_ORBIT)
        {
            craft->state = CS_ORBIT_DOCKING;
            craft->state_timer = CSTD_DOCK;
        }
    }
    if (IsKeyPressed(KEY_A))
    {
        // ascend / descend
        switch (craft->state)
        {
        case CS_ORBIT:
            craft->state = CS_DESCENDING;
            craft->state_timer = CSTD_DESCENT;
            break;
        case CS_SURFACE:
        case CS_SURFACE_DOCKED:
            craft->state = CS_SURFACE_LAUNCH;
            craft->state_timer = CSTD_LAUNCH;
            break;
        default:
            break;
        }
    }

    auto &Overlay = Overlay::getInstance();

    // transparent buttons seem like overkill, reimplement
    if ((craft->type != CT_SHUTTLE) && (craft->drive))
    {
        auto &source{uiElementSources[UI_DRIVE_CONTROLS]};
        const Rectangle driveButton{1127, 640, source.width * 4, source.height * 2};
        if (Overlay.clickedArea(driveButton, "Engage drive"))
        {
            craft->engageDrive();
        }
        const Rectangle disengageDriveButton{1127, 640 + source.height * 2, source.width * 4, source.height * 2};
        if (Overlay.clickedArea(disengageDriveButton, "Disengage drive"))
        {
            craft->disengageDrive();
        }
    }

    // if has weapon
    if ((craft->type != CT_SHUTTLE) && craft->pods[0].type == PT_WEAPON)
    {
        auto &source{uiElementSources[UI_DRONE_CONTROLS]};
        const Rectangle droneButton{1280 - source.width * 4, 838, source.width * 4, source.height * 4};
        if (Overlay.clickedArea(droneButton, "Activate drone computer"))
        {
            droneControlView->activate(craft);
        }
    }

    // engines
    if (IsKeyPressed(KEY_E))
    {
        craft->engageDrive();
    }

    if (IsKeyPressed(KEY_X))
    {
        // enable/disable autopilot
        craft->autopilot->state = (craft->autopilot->state == AS_ON) ? AS_OFF : AS_ON;
    }

    if (IsKeyPressed(KEY_Z))
    {
        // configure autopilot
        if (autopilotView)
        {
            autopilotView->visible = !autopilotView->visible;
        }
    }

    if (IsKeyPressed(KEY_T))
    {
        // test - set a destination
        // lazy create destination picker on demand
        if (!destinationPicker)
        {
            destinationPicker.reset(DestinationPicker::create(location->system, (Vector2){600, 500}, 0.5f)); // owns picker
            destinationPicker->setCallbacks(this, destinationSelected, destinationSelectCancelled);          // allow picker to call back to shuttle view when destination selected
        }

        destinationPicker->visible = !destinationPicker->visible;
    }

    if (destinationPicker && destinationPicker->visible)
    {
        destinationPicker->input();
    }

    droneControlView->input();
}

void ShuttleView::render()
{
    BasePage::render();

    // render viewport
    if (bodyTexture && (craft->state == CS_ORBIT))
    {
        // test rendering 1/4 of a body, 256 x 256
        Rectangle source{128, 128, 128, 128};
        Rectangle dest{320, 200, 512, 512};
        DrawTexturePro(*bodyTexture, source, dest, (Vector2){0, 0}, 0.f, WHITE);

        // orbital?
        if (Game::getCurrent()->orbitalAt(craft->location))
        {
            Rectangle ssource{1229, 179, 82, 72};
            Rectangle sdest{600, 460, 320, 280};
            DrawTexturePro(*backgroundTexture, ssource, sdest, (Vector2){0, 0}, 0.f, WHITE);
        }
    }

    if (backgroundTexture)
    {
        if (craft->state != CS_ORBIT)
        {
            DrawTexturePro(*backgroundTexture, viewportImages[craft->state], viewportDest, (Vector2){0, 0}, 0.f, WHITE);
        }
    }

    char status[128];

    DrawText(craft->statusText(status, sizeof status), 320, 160, 20, YELLOW);

    // if have a destination, show that too
    if (craft->type != CT_SHUTTLE)
    {
        if (craft->currentDestination().location)
        {
            char dest_status[128];
            std::snprintf(dest_status, sizeof dest_status, "Destination: %s", craft->currentDestination().location->name);
            DrawText(dest_status, 320, 190, 20, YELLOW);
            if (craft->state == CS_TRANSIT)
            {
                float progress = craft->total_state_timer > 0.0f ? craft->state_timer / craft->total_state_timer : 0.0f;
                std::snprintf(dest_status, sizeof dest_status, "Progress: %.0f%%", (1.0f - progress) * 100.0f);
                DrawText(dest_status, 320, 220, 20, YELLOW);
            }
        }
    }

    // controls and possibly clickable status for pods
    Overlay &overlay = Overlay::getInstance(); // get the overlay instance to set tooltips when hovering buttons

    // pods
    float y{160};
    for (int idx = 0; idx < craft->max_pods; ++idx)
    {
        DrawText(craft->pods[idx].description(status, sizeof status), 900, y, 20, YELLOW);
        y += 24;

        // pod icons, potentially clickable
        // render for tool, supply, cryo. Not empty or weapon (weapons have their own UI).
        auto pt{craft->pods[idx].type};
        if ((pt > PT_EMPTY) && (pt < PT_WEAPON))
        {
            // by construction, the icon image element ID happens to equal the pod type
            DrawTexturePro(*itemsTexture, uiElementSources[craft->pods[idx].type], pod_icon_coordinates[idx], (Vector2){0, 0}, 0.f, WHITE);

            // if is tool pod, and can activate in current state:
            bool canActivate = (craft->pods[idx].type == PT_TOOL) && Game::getCurrent()->canActivatePod(craft, idx);
            // check game logic for activation conditions

            if (canActivate)
            {
                // add hovertext
                if (overlay.addToolTip("Activate", pod_icon_coordinates[idx]) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    // clicked on pod icon, activate
                    Game::getCurrent()->activatePod(craft, idx);
                }
            }
        }
    }

    // controls

    // render control buttons (normal rendering)
    if ((craft->type != CT_SHUTTLE) && (craft->drive))
    {
        // has interorbit drive
        auto &source{uiElementSources[UI_DRIVE_CONTROLS]};
        const Rectangle driveButton{1127, 640, source.width * 4, source.height * 4};
        DrawTexturePro(*uiTexture, source, driveButton, (Vector2){0, 0}, 0.f, WHITE);
    }

    // if has weapon
    if ((craft->type != CT_SHUTTLE) && craft->pods[0].type == PT_WEAPON)
    {
        // has weapon in pod 0
        auto &source{uiElementSources[UI_DRONE_CONTROLS]};
        const Rectangle droneButton{1280 - source.width * 4, 838, source.width * 4, source.height * 4};
        DrawTexturePro(*uiTexture, source, droneButton, (Vector2){0, 0}, 0.f, WHITE);
    }

    {
        UITransparentButtonState transparentButtonState;
        // dock (561,838 - 633, 890)
        // descend (642, 831 - 718, 888)
        // ascend (722, 832 - 798, 881)
        const Rectangle dockButton{561, 838, 72, 52};
        const Rectangle descendButton{642, 831, 76, 57};
        const Rectangle ascendButton{722, 832, 76, 49};

        // can dock if: in orbit, there is an orbital, it is complete
        Game *game = Game::getCurrent();
        bool can_dock = false;
        if (craft->state == CS_ORBIT)
        {
            if (Orbital *o = game->orbitalAt(craft->location))
            {
                can_dock = o->operational;
            }
        }

        if (can_dock && (overlay.renderButton(dockButton, "", "Dock", WHITE)))
        {
            craft->state = CS_ORBIT_DOCKING;
            craft->state_timer = CSTD_DOCK;
        }
        if ((craft->state == CS_ORBIT_DOCKED) && (overlay.renderButton(dockButton, "", "Undock", WHITE)))
        {
            craft->state = CS_ORBIT_LAUNCH;
            craft->state_timer = CSTD_LAUNCH;
        }

        // can descend IF in orbit and a shuttle
        bool can_descend = (craft->state == CS_ORBIT) && (craft->type == CT_SHUTTLE);
        if (can_descend && (overlay.renderButton(descendButton, "", "Descend to surface", WHITE)))
        {
            craft->state = CS_DESCENDING;
            craft->state_timer = CSTD_DESCENT;
        }
        if (((craft->state == CS_SURFACE) || (craft->state == CS_SURFACE_DOCKED)) && (overlay.renderButton(ascendButton, "", "Ascend to orbit", WHITE)))
        {
            craft->state = CS_SURFACE_LAUNCH;
            craft->state_timer = CSTD_LAUNCH;
        }
    }
    // autopilot config
    if (autopilotView && autopilotView->visible)
    {
        autopilotView->render();
    }

    if (destinationPicker && destinationPicker->visible)
    {
        destinationPicker->render();
    }

    droneControlView->render();

    pageLog.render();
}

void ShuttleView::update(const float delta)
{
    pageLog.update(delta);
}

void ShuttleView::onOrbitalConstruction(Orbital *orbital)
{
    char buffer[256];
    if (craft->location == orbital->location)
    {
        if (orbital->operational)
        {
            std::snprintf(buffer, sizeof buffer, "Orbital construction complete at location %s", craft->location->name);
            pageLog.addLog(buffer);
        }
        else
        {
            std::snprintf(buffer, sizeof buffer, "Orbital construction progress at location %s: %d%%", craft->location->name, orbital->construction_progress * 100 / 8);
            pageLog.addLog(buffer);
        }
    }
}