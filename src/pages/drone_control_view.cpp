#include "pages/drone_control_view.h"
#include "state/game.h"
#include "pages/overlay.h"
#include "assets/textures.h"

#include <cstdio>

extern "C"
{
#include "raylib.h"
}

void DroneControlView::activate(Craft *c)
{
    craft = c;
    droneType = Game::getCurrent()->droneTypeForCraft(craft);
    visible = true;
    state = DCS_MANAGE;
}

void DroneControlView::deactivate()
{
    craft = nullptr;
    droneType = MAX_ITEM_TYPE;
    visible = false;
}

void DroneControlView::input()
{
    if (!visible)
    {
        return;
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        // toggle between manage and battle mode on right click
        deactivate();
    }
}

void DroneControlView::render()
{
    if (!visible)
    {
        return;
    }

    // add a dimmed background to make it clear this is an overlay
    DrawRectangle(left, top, GetScreenWidth() - left, GetScreenHeight() - top, (Color){0, 0, 0, 128});

    switch (state)
    {
    case DCS_MANAGE:
        render_manage();
        break;
    case DCS_BATTLE:
        render_battle();
        break;
    default:
        TraceLog(LOG_ERROR, "Invalid drone control state");
        break;
    }
}

void DroneControlView::render_manage()
{
    static auto docImageTexture = TextureManager::getInstance().getTexture(TEXTURE_ITEMS);
    const Rectangle itemImageTarget = {(float)(left + 400.0), (float)(top + 60.0), 256, 224};

    DrawText("Drone Control - Manage", left, top, 20, WHITE);

    // can draw drones from orbital fleet and add to craft fleet
    // or return drones from craft fleet to orbital fleet
    // fleet type depends on craft type ATM
    // call that game logic - what kind of drone to use by craft type
    if (droneType == MAX_ITEM_TYPE)
    {
        DrawText("No drones available for this craft", left, top + 30, 20, YELLOW);
    }
    else
    {
        Game *game = Game::getCurrent();
        char buffer[64];
        auto drone_count = game->droneCountForCraft(craft);
        std::snprintf(buffer, sizeof buffer, "Fleet Drones: %d", drone_count);
        DrawText(buffer, (float)left, (float)(top + 30), 20, YELLOW);

        // drone image - production image 3
        auto source_rect = itemImageSources[II_PROD_IOSDRONE3];
        DrawTexturePro(*docImageTexture, source_rect, itemImageTarget, (Vector2){0, 0}, 0.f, WHITE);

        // if at an orbital, can add/remove drones
        if (craft->state == CS_ORBIT)
        {
            if (Orbital *o = game->orbitalAt(craft->location))
            { // check if at orbital, and if so show add/remove buttons
                auto &overlay{Overlay::getInstance()};

                auto orbital_drones = o->stores.items[droneType];
                std::snprintf(buffer, sizeof buffer, "Orbital Drones: %d", orbital_drones);
                DrawText(buffer, left, top + 110, 20, YELLOW);

                if (overlay.renderButton({(float)left, (float)top + 60, 120, 30}, "Add All", "Add All Drones To Fleet", WHITE))
                {
                    if ((drone_count < MAX_DRONE_FLEET_SIZE) && (orbital_drones > 0))
                    {
                        // maximise fleet drones based on available orbital drones and max fleet size
                        int can_add = std::min(orbital_drones, MAX_DRONE_FLEET_SIZE - drone_count);
                        craft->pods[0].amount += can_add;
                        o->stores.items[droneType] -= can_add;
                    }
                }

                if (overlay.renderButton({(float)left + 130, (float)top + 60, 120, 30}, "Add", "Add Drone To Fleet", WHITE))
                {
                    if ((drone_count < MAX_DRONE_FLEET_SIZE) && (orbital_drones > 0))
                    {
                        // transfer one drone from orbital to fleet
                        craft->pods[0].amount += 1;
                        o->stores.items[droneType] -= 1;
                    }
                }
                if (overlay.renderButton({(float)left + 260, (float)top + 60, 120, 30}, "Remove", "Return Drone To Orbital", WHITE))
                {
                    if (drone_count > 0)
                    {
                        // transfer one drone from fleet to orbital
                        craft->pods[0].amount -= 1;
                        o->stores.items[droneType] += 1;
                    }
                }
            } // has orbital

        } // orbit
    }
}

void DroneControlView::render_battle()
{
    DrawText("Drone Control - Battle", left, top, 20, WHITE);
}