#include "pages/drone_control_view.h"
#include "state/game.h"
#include "pages/overlay.h"
#include "assets/textures.h"
#include "pages/pages.h"
#include "assets/ui_elements.h"

#include <cstdio>
#include <cmath>

extern "C"
{
#include "raylib.h"
}

void DroneControlView::activate(Craft *c)
{
    auto game = Game::getCurrent();
    craft = c;
    location = c->location;
    droneType = game->droneTypeForCraft(craft);
    visible = true;
    state = DCS_MANAGE;

    if (location)
    {
        auto current_faction_id = PageManager::getInstance().viewState.getFactionId();
        // check for hostiles
        if (game->hostilesAt(location, current_faction_id))
        {
            state = DCS_BATTLE;
            current_orbital = game->orbitalAt(location); // TODO depends on aggressor
            if (current_orbital)
            {
                orbital_drone_count = current_orbital->stores.items[droneType]; // TODO could defend with more than one
            }
            fleet_drone_count = game->droneCountForCraft(craft);

            // choose the motion model here (factory-selected). change to MP_HELICAL to
            // compare; both fleets use the same pattern so they read consistently.
            const MovementPatternType pattern = MP_FLOCKING;
            attackers.initialise(fleet_drone_count, RED, pattern);
            defenders.initialise(orbital_drone_count, BLUE, pattern);

            // place the fleets in battle-area-local coords (render() offsets by left/top).
            // attackers sweep in from the left, defenders from the right; both converge on centre.
            float area_w = (float)(GetScreenWidth() - 2 * left);
            float area_h = (float)(GetScreenHeight() - 2 * top);
            float centre_x = area_w * 0.5f;
            float centre_y = area_h * 0.5f;
            attackers.setApproach({area_w * 0.08f, centre_y}, centre_x, +1.0f);
            defenders.setApproach({area_w * 0.92f, centre_y}, centre_x, -1.0f);
        }
    }
}

void DroneControlView::deactivate()
{
    craft = nullptr;
    droneType = ItemType::None;
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

    // is there a defending (left) or target (right) orbital?
    int render_orbital_x = -1;
    Game *game = Game::getCurrent();
    auto &pm{PageManager::getInstance()};
    auto width = GetScreenWidth() - 2 * left;
    if (location)
    {
        Orbital *o = game->orbitalAt(location);
        if (o)
        {
            char buffer[256];
            if (o->faction_id == pm.viewState.getFactionId())
            {
                snprintf(buffer, sizeof buffer, "Defending %s Orbital", location->name);
                render_orbital_x = left + 20;
            }
            else
            {
                snprintf(buffer, sizeof buffer, "Target: %s Orbital", location->name);
                render_orbital_x = left + width - 80;
            }
            DrawText(buffer, left + width / 2, top, 20, YELLOW);
        }
    }
    if (render_orbital_x != -1)
    {
        // render a small orbital image next to the label
        static auto docImageTexture = TextureManager::getInstance().getTexture(TEXTURE_UI_BUTTONS);
        const Rectangle itemImageTarget = {(float)render_orbital_x, (float)(top + 250), 48 * 4, 16 * 4};
        DrawTexturePro(*docImageTexture, uiElementSources[UI_BUTTON_ORBITAL], itemImageTarget, (Vector2){0, 0}, 0.f, WHITE);
    }

    // render fleet count, top left
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "Fleet Drones: %d", fleet_drone_count);
    DrawText(buffer, (float)left, (float)(top + 30), 20, YELLOW);

    // render orbital count if have an orbital, top right
    if (current_orbital)
    {
        std::snprintf(buffer, sizeof buffer, "Orbital Drones: %d", orbital_drone_count);
        DrawText(buffer, (float)(left + width - 200), (float)(top + 30), 20, YELLOW);
    }

    // flee button

    // fleets
    attackers.render(top, left);
    defenders.render(top, left);
}

void DroneControlView::update(float delta)
{
    attackers.update(delta);
    defenders.update(delta);
}

// DroneFleetMarkers rendering. State + the motion driver (initialise/setApproach/update/
// killRandom/compact) live in fleet_movement.cpp; only the draw pass belongs here.

void DroneFleetMarkers::render(int t, int l)
{
    for (int i = 0; i < out_count; i++)
    {
        Vector2 p = {out_pos[i].x + l, out_pos[i].y + t};
        float r = 3.0f + 4.0f * out_depth[i]; // ~3px (behind) .. ~7px (front)
        DrawCircleV(p, r, color);
    }
}