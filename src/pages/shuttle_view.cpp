#include "pages/shuttle_view.h"
#include "pages/overlay.h"
#include "assets/ui_elements.h"
#include "state/game.h"
#include "state/location.h"
#include "state/autopilot.h"
#include "state/craft_action.h"

// original images 208 x 120 -> 832 x 480
Rectangle viewportDest = {300, 200, 832, 480};

// Viewport art. A single state can no longer choose this: since the collapse, CS_IDLE
// and CS_WORKING each cover docked, in-orbit and on-surface, so the PLACE decides for
// those. The manoeuvres are the other way round -- they look the same wherever they
// happen -- so the STATE decides for them. Hence the two halves of imageFor() below.
namespace
{
    const Rectangle VIEWPORT_DOCKED{1368, 280, 208, 120};
    const Rectangle VIEWPORT_ORBIT{1368, 152, 208, 120}; // orbit, no station
    const Rectangle VIEWPORT_STORM_DOORS{1152, 280, 208, 120};
    const Rectangle VIEWPORT_TRANSIT{1152, 408, 208, 120};
    const Rectangle VIEWPORT_ASTEROIDS{1152, 536, 208, 120};

    // The orbit region draws a rendered planet with an optional station overlay instead
    // of flat art -- but only at rest. Mid-manoeuvre the flat image stays up, so the
    // view does not flick to the planet while docking or descending, and so launching
    // out of an orbital still shows its storm doors.
    bool showsPlanetView(const Craft *craft)
    {
        return craft->location &&
               craft->location->type == LOCATION_TYPE_ORBIT &&
               !craft->moving();
    }

    Rectangle viewportImageFor(const Craft *craft)
    {
        if (craft->location->type == LOCATION_TYPE_ASTEROID_BELT)
        {
            return VIEWPORT_ASTEROIDS; // asteroid belt is a region, not a body, so it is always orbit
        }

        // Manoeuvres first: what the craft is doing outranks where it happens to be.
        if (craft->inTransit())
        {
            return VIEWPORT_TRANSIT;
        }
        if (craft->isLaunching())
        {
            return VIEWPORT_STORM_DOORS;
        }
        if (craft->moving())
        {
            // ascending, descending or docking -- between two places rather than at
            // either, so no station overlay is implied.
            return VIEWPORT_ORBIT;
        }

        // At rest or working, so the place answers it. Docked is any facility, on
        // either side of the body.
        if (craft->docked())
        {
            return VIEWPORT_DOCKED;
        }
        if (craft->inOrbit())
        {
            return VIEWPORT_ORBIT;
        }
        // Surface region, undocked. No surface art in the atlas yet, so it borrows the
        // docked image as the old per-state table did.
        return VIEWPORT_DOCKED;
    }
}

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
        location = craft->body(); // the page shows a body, and seeds the picker from its system
    }
    else if (auto l = viewState.getCurrentBody())
    {
        location = l;
        craft = location->shuttle; // the shuttle is registered on the body
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
        // The picker offers bodies; setDestination resolves one to an exact place --
        // the orbital if there is one, its orbit region otherwise.
        craft->setDestination(craft->destination_index, loc);

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
    if (!craft)
    {
        return; // see render(): no craft focused, nothing to command
    }

    auto can_dock = craft->canDock();
    auto can_launch = craft->canLaunch();
    if (IsKeyPressed(KEY_D))
    {
        // dock/undock
        if (can_launch)
        {
            craft->launch();
        }
        else if (can_dock)
        {
            craft->dock();
        }
    }

    if (IsKeyPressed(KEY_A))
    {
        if (craft->canDescend())
        {
            craft->descend();
        }
        else if (craft->canAscend())
        {
            craft->ascend();
        }
    }

    auto &Overlay = Overlay::getInstance();

    // transparent buttons seem like overkill, reimplement
    auto can_engage_drive = craft->canEngageDrive();

    if ((craft->hasCapability(CC_INTERPLANETARY)))
    {
        auto &source{uiElementSources[UI_DRIVE_CONTROLS]};
        const Rectangle driveButton{1127, 640, source.width * 4, source.height * 2};
        if (((Overlay.clickedArea(driveButton, can_engage_drive ? "Engage drive" : can_engage_drive.text())) || IsKeyPressed(KEY_E)) && can_engage_drive)
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

    // autopilot

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

    if (IsKeyPressed(KEY_T) && (craft->hasCapability(CC_INTERPLANETARY)))
    {
        // test - set a destination
        // lazy create destination picker on demand

        if (!location)
        {
            TraceLog(LOG_ERROR, "No location for craft to pick destination in"); // TODO - associate to system, or space location in system?
            return;
        }

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

    // activate() leaves craft null when nothing is focused and the body has no shuttle,
    // and everything below dereferences it. The sidebar has already drawn, so an empty
    // cockpit is the right result.
    if (!craft)
    {
        return;
    }

    // set common state
    auto craft_can_dock = craft->canDock();

    // render viewport -- exactly one of these two, they are complements
    if (showsPlanetView(craft))
    {
        if (bodyTexture)
        {
            // test rendering 1/4 of a body, 256 x 256
            Rectangle source{128, 128, 128, 128};
            Rectangle dest{320, 200, 512, 512};
            DrawTexturePro(*bodyTexture, source, dest, (Vector2){0, 0}, 0.f, WHITE);
        }

        // A station in this orbit is drawn over the planet rather than being a separate
        // image, so a second orbital at the same body needs no new art.
        if (backgroundTexture && Game::getCurrent()->orbitalAt(craft->location))
        {
            Rectangle ssource{1229, 179, 82, 72};
            Rectangle sdest{600, 460, 320, 280};
            DrawTexturePro(*backgroundTexture, ssource, sdest, (Vector2){0, 0}, 0.f, WHITE);
        }
    }
    else if (backgroundTexture)
    {
        DrawTexturePro(*backgroundTexture, viewportImageFor(craft), viewportDest, (Vector2){0, 0}, 0.f, WHITE);
    }

    char status[128];

    DrawText(craft->statusText(status, sizeof status), 320, 160, 20, YELLOW);

    // crew (224,866)
    if (craft->crew)
    {
        char crew_status[128];
        DrawText(craft->crew->description(crew_status, sizeof crew_status), 224, 866, 20, YELLOW);
        std::snprintf(status, sizeof status, "%d marines", craft->crew->size);
        DrawText(status, 224, 886, 20, YELLOW);
    }

    // if have a destination, show that too
    if (craft->hasCapability(CC_INTERPLANETARY))
    {
        if (craft->currentDestination().location)
        {
            char dest_status[128];
            std::snprintf(dest_status, sizeof dest_status, "Destination: %s", craft->currentDestination().location->name);
            DrawText(dest_status, 320, 190, 20, YELLOW);
            if (craft->inTransit())
            {
                float progress = craft->stateProgress();
                std::snprintf(dest_status, sizeof dest_status, "Progress: %.0f%%", progress * 100.0f);
                DrawText(dest_status, 320, 220, 20, YELLOW);
            }
        }
    }

    // scanning status. Separate buffer: formatting `status` from itself overlaps source
    // and destination, which snprintf does not define.
    if (craft->scanning())
    {
        char target[128];
        craft->scanTargetText(target, sizeof target);
        std::snprintf(status, sizeof status, "Scanning: %s", target);
        DrawText(status, 320, 250, 20, YELLOW);
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
    if (craft->hasCapability(CC_INTERPLANETARY))
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
        // if of a different faction: can dock if not hostile. If hostile, can dock if no defenders (drones at orbital)
        Game *game = Game::getCurrent();

        if (craft_can_dock && (overlay.renderButton(dockButton, "", "Dock", WHITE)))
        {
            craft->dock();
        }
        if ((craft->canLaunch()) && (overlay.renderButton(dockButton, "", "Undock", WHITE)))
        {
            craft->launch();
        }

        // can descend IF in orbit and a shuttle
        auto can_descend = craft->canDescend();
        if ((overlay.renderButton(descendButton, "", can_descend ? "Descend to surface" : can_descend.text(), can_descend ? WHITE : BLUE)) && can_descend)
        {
            craft->descend();
        }
        auto can_ascend = craft->canAscend();
        if ((overlay.renderButton(ascendButton, "", can_ascend ? "Ascend to orbit" : can_ascend.text(), can_ascend ? WHITE : BLUE)) && can_ascend)
        {
            craft->ascend();
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
    droneControlView->update(delta);
}

void ShuttleView::renderDebug()
{
    if (droneControlView)
    {
        droneControlView->renderDebug();
    }
}

void ShuttleView::onOrbitalConstruction(Orbital *orbital)
{
    char buffer[256];
    if (craft->body() == orbital->body())
    {
        if (orbital->operational)
        {
            std::snprintf(buffer, sizeof buffer, "Orbital construction complete at location %s", craft->body()->name);
            pageLog.addLog(buffer);
        }
        else
        {
            std::snprintf(buffer, sizeof buffer, "Orbital construction progress at location %s: %d%%", craft->location->name, orbital->construction_progress * 100 / 8);
            pageLog.addLog(buffer);
        }
    }
}