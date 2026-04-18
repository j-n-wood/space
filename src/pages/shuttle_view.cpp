#include "pages/shuttle_view.h"
#include "state/location.h"

// original images 208 x 120 -> 832 x 480
Rectangle viewportDest = {300, 200, 832, 480};

// source images for states
/*
    CS_SURFACE, // surface no dock
    CS_SURFACE_DOCKED,
    CS_SURFACE_WORK,
    CS_SURFACE_LAUNCH, // transient state leaving dock
    CS_ASCENDING,
    CS_ORBIT,
    CS_ORBIT_DOCKING, // transient state entering dock
    CS_ORBIT_DOCKED,
    CS_ORBIT_WORK,
    CS_ORBIT_LAUNCH, // transient state leaving dock
    CS_DESCENDING,
    CS_TRANSIT, // IP or IS transit - refine with type and speed
    CS_COUNT
*/
// this won't do - image depends on location properties e.g. station presence (could overlay)
// or location type (star, planet, etc)
Rectangle viewportImages[CS_COUNT] = {
    {1368, 280, 208, 120}, // docked
    {1368, 280, 208, 120}, // docked
    {1368, 280, 208, 120}, // docked
    {1368, 280, 208, 120}, // docked
    {1152, 280, 208, 120}, // storm doors
    {1368, 152, 208, 120}, // orbit no station
    {1368, 152, 208, 120}, // orbit no station
    {1368, 280, 208, 120}, // docked
    {1368, 152, 208, 120}, // orbit no station
    {1152, 280, 208, 120}, // storm doors
    {1368, 152, 208, 120}, // orbit no station
    {1152, 408, 208, 120}, // transit
};

void ShuttleView::activate(ViewState &viewState)
{
    if (auto l = viewState.getCurrentLocation())
    {
        location = l;
        shuttle = location->shuttle.get();
    }
}

void ShuttleView::input()
{
    if (IsKeyPressed(KEY_D))
    {
        // dock/undock
        if (shuttle->state == CS_ORBIT_DOCKED)
        {
            shuttle->state = CS_ORBIT_LAUNCH;
        }
        else if (shuttle->state = CS_ORBIT)
        {
            shuttle->state = CS_ORBIT_DOCKING;
        }
    }
    if (IsKeyPressed(KEY_A))
    {
        // ascend / descend
        switch (shuttle->state)
        {
        case CS_ORBIT:
            shuttle->state = CS_DESCENDING;
            shuttle->state_timer = 2;
            break;
        case CS_SURFACE:
        case CS_SURFACE_DOCKED:
            shuttle->state = CS_SURFACE_LAUNCH;
            break;
        }
    }
}

void ShuttleView::render()
{
    BasePage::render();

    // render viewport
    if (backgroundTexture)
    {
        DrawTexturePro(*backgroundTexture, viewportImages[shuttle->state], viewportDest, (Vector2){0, 0}, 0.f, WHITE);
    }

    char status[128];
    switch (shuttle->state)
    {
    case CS_SURFACE: // surface no dock
        std::snprintf(status, sizeof status, "On surface of %s", location->name);
        break;
    case CS_SURFACE_DOCKED:
        std::snprintf(status, sizeof status, "Docked at %s station", location->name);
        break;
    case CS_SURFACE_WORK:
        std::snprintf(status, sizeof status, "Working on %s surface", location->name);
        break;
    case CS_SURFACE_LAUNCH:
        std::snprintf(status, sizeof status, "Launching from %s", location->name);
        break;
    case CS_ASCENDING:
        std::snprintf(status, sizeof status, "Ascending from %s", location->name);
        break;
    case CS_ORBIT:
        std::snprintf(status, sizeof status, "Orbiting %s", location->name);
        break;
    case CS_ORBIT_DOCKING:
        std::snprintf(status, sizeof status, "Docking with %s orbital", location->name);
        break;
    case CS_ORBIT_DOCKED:
        std::snprintf(status, sizeof status, "Docked at %s orbital", location->name);
        break;
    case CS_ORBIT_WORK:
        std::snprintf(status, sizeof status, "Working in %s orbit", location->name);
        break;
    case CS_ORBIT_LAUNCH:
        std::snprintf(status, sizeof status, "Launching from %s orbital", location->name);
        break;
    case CS_DESCENDING:
        std::snprintf(status, sizeof status, "Descending to %s", location->name);
        break;
    case CS_TRANSIT:
        std::snprintf(status, sizeof status, "In transit");
        break;
    }

    DrawText(status, 320, 160, 20, YELLOW);
}