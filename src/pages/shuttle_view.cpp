#include "pages/shuttle_view.h"
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
    }
    else
    {
        std::snprintf(title, sizeof title, "No Shuttle");
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

    // pods
    float y{160};
    for (int idx = 0; idx < craft->max_pods; ++idx)
    {
        DrawText(craft->pods[idx].description(status, sizeof status), 900, y, 20, YELLOW);
        y += 24;
    }
}