#include "pages/overlay.h"
#include "state/game.h"
#include "pages/pages.h"
#include <cstdio>

extern "C"
{
#include "raygui/raygui.h"
}

Overlay::Overlay() : toolTipSet(false)
{
    setDefaultStyle();
}

void Overlay::start()
{
    // any setup that needs to be done when the overlay is first created can be done here
    currentToolTip[0] = '\0'; // reset tooltip when starting the overlay
    toolTipSet = false;
}

bool Overlay::clickedArea(const Rectangle &area, const char *toolTip)
{
    if (CheckCollisionPointRec(GetMousePosition(), area))
    {
        setCurrentToolTip(toolTip);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            return true;
        }
    }
    return false;
}

int Overlay::renderButton(const Rectangle &buttonRect, const char *buttonText, const char *toolTip, const Color &color)
{
    // Implementation for rendering a button with hover text
    // Check if mouse is hovering over the button
    if ((toolTip != nullptr) && (CheckCollisionPointRec(GetMousePosition(), buttonRect)))
    {
        setCurrentToolTip(toolTip); // set the current tooltip to be displayed by the overlay
    }
    return GuiButton(buttonRect, buttonText);
}

// render a button with a hover callback if should be in hover state
int Overlay::renderButtonHover(const Rectangle &buttonRect, const char *buttonText, const Color &color, onHover hover, void *state)
{
    if (CheckCollisionPointRec(GetMousePosition(), buttonRect))
    {
        hover(state);
    }
    return GuiButton(buttonRect, buttonText);
}

void Overlay::render()
{
    // common status indicators, hover text, etc. that should be drawn on top of all pages can be rendered here
    // renders after the current page is rendered, so will appear on top of page content

    auto game{Game::getCurrent()};

    auto &pm{PageManager::getInstance()};

    // allow for auto-update of state if following a craft and it arrives at a location
    // TODO use a subscription method to avoid checking this every frame
    auto craft = pm.viewState.getCurrentCraft();
    if (craft)
    {
        auto craftLocation = craft->location;
        if (craftLocation != pm.viewState.getCurrentLocation())
        {
            pm.viewState.setCurrentLocation(craftLocation);
            if (craftLocation)
            {
                pm.viewState.setCurrentSystem(craftLocation->system); // system could change eventually
                pm.viewState.setCurrentFacility(game->facilityAt(Endpoint(craftLocation, SLOC_ORBIT, true)));
            }
        }
    }

    DrawText(pm.viewState.getCurrentSystem()->name, 10, 10, 20, WHITE);
    if (auto location = pm.viewState.getCurrentLocation())
    {
        DrawText(location->name, 100, 10, 20, WHITE);
    }
    DrawText((PageManager::getInstance()).getCurrentPage()->title, 200, 10, 20, WHITE);

    if (toolTipSet)
    {
        auto mp = GetMousePosition();
        DrawText(currentToolTip, mp.x + 12, mp.y + 8, 20, WHITE);
    }

    // time
    char buf[256];
    sprintf(buf, "%.2f", game->game_time);
    DrawText(buf, BasePage::timeDest.x, BasePage::timeDest.y, 20, WHITE);

    // console
    if (console)
    {
        Rectangle consoleRect = {50, 950, 400, 50};
        GuiGroupBox(consoleRect, "Console");
        Rectangle inputRect = {consoleRect.x + 10, consoleRect.y + consoleRect.height - 40, consoleRect.width - 20, 30};
        GuiTextBox(inputRect, consoleInput, sizeof(consoleInput), true);
        if (IsKeyPressed(KEY_ENTER))
        {
            // process console command in consoleInput
            TraceLog(LOG_INFO, "Console command entered: %s", consoleInput);

            // give {item id} {amount} command to add items to current location for testing
            int item_id{ItemType::None};
            int amount{0};
            if (std::sscanf(consoleInput, "give %d %d", &item_id, &amount) == 2)
            {
                // TODO move to game state
                Facility *f{pm.viewState.getCurrentFacility()};
                f->stores.items[item_id] += amount;
                TraceLog(LOG_INFO, "Added %d of item %d to current location %s", amount, item_id, f->location->name);
            }
            // 'orbital' command to create an orbital at the current location for testing, if not present
            else if (std::strcmp(consoleInput, "orbital") == 0)
            {
                Location *loc{pm.viewState.getCurrentLocation()};
                if (!game->orbitalAt(loc))
                {
                    Orbital *orbital{game->createOrbital(loc)};
                    orbital->operational = true;
                    TraceLog(LOG_INFO, "Orbital created at location: %s", loc->name);
                }
            }
            // 'shuttle' to spawn shuttle at current facility if any
            else if (std::strcmp(consoleInput, "shuttle") == 0)
            {
                Facility *f{pm.viewState.getCurrentFacility()};
                if (f && !game->locationHasShuttle(f->location)) // location check is probably redundant as facility should always have a location, but just in case
                {
                    Shuttle *shuttle{game->createShuttle(f)};
                    TraceLog(LOG_INFO, "Shuttle created at facility: %s", f->location->name);
                }
            }
            // research {topic id} command to set research progress for testing
            else if (std::sscanf(consoleInput, "research %d", &item_id) == 1)
            {
                if (item_id == -1)
                {
                    // set all research to complete
                    for (auto &topic : game->researchTopics)
                    {
                        topic.progress = topic.requiredTime;
                    }
                    // set all items to researched as well
                    for (auto &item : game->items)
                    {
                        item.researched = true;
                    }
                    TraceLog(LOG_INFO, "All research topics set to complete");
                }
                else if (item_id >= 0 && item_id < game->researchTopics.size())
                {
                    game->researchTopics[item_id].progress = game->researchTopics[item_id].requiredTime;
                    TraceLog(LOG_INFO, "Research topic %d set to complete", item_id);
                }
            }

            // clear input
            consoleInput[0] = '\0';
        }
    }
}

void Overlay::setDefaultStyle()
{
    /*
           BORDER_COLOR_NORMAL = 0,    // Control border color in STATE_NORMAL
       BASE_COLOR_NORMAL,          // Control base color in STATE_NORMAL
       TEXT_COLOR_NORMAL,          // Control text color in STATE_NORMAL
       BORDER_COLOR_FOCUSED,       // Control border color in STATE_FOCUSED
       BASE_COLOR_FOCUSED,         // Control base color in STATE_FOCUSED
       TEXT_COLOR_FOCUSED,         // Control text color in STATE_FOCUSED
       BORDER_COLOR_PRESSED,       // Control border color in STATE_PRESSED
       BASE_COLOR_PRESSED,         // Control base color in STATE_PRESSED
       TEXT_COLOR_PRESSED,         // Control text color in STATE_PRESSED
       BORDER_COLOR_DISABLED,      // Control border color in STATE_DISABLED
       BASE_COLOR_DISABLED,        // Control base color in STATE_DISABLED
       */

    int defaultBorderColor = 0x003333ff;
    int defaultBaseColor = 0x002222ff;
    int defaultTextColor = 0xCCCCCCff;

    int focusedBorderColor = 0x005555ff;
    int focusedBaseColor = 0x004444ff;
    int focusedTextColor = 0xDDCCDDff;

    int pressedBorderColor = 0x008888ff;
    int pressedBaseColor = 0x005555ff;
    int pressedTextColor = 0xEECCEEff;

    int disabledBorderColor = 0x002222ff;
    int disabledBaseColor = 0x000000ff;
    int disabledTextColor = 0x888888ff;

    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, defaultBorderColor);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, defaultBaseColor);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, defaultTextColor);

    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, focusedBorderColor);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, focusedBaseColor);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, focusedTextColor);

    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, pressedBorderColor);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, pressedBaseColor);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, pressedTextColor);

    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, disabledBorderColor);
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, disabledBaseColor);
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, disabledTextColor);

    GuiSetStyle(COMBOBOX, BORDER_COLOR_NORMAL, defaultBorderColor);
    GuiSetStyle(COMBOBOX, BASE_COLOR_NORMAL, defaultBaseColor);
    GuiSetStyle(COMBOBOX, TEXT_COLOR_NORMAL, defaultTextColor);

    GuiSetStyle(COMBOBOX, BORDER_COLOR_FOCUSED, focusedBorderColor);
    GuiSetStyle(COMBOBOX, BASE_COLOR_FOCUSED, focusedBaseColor);
    GuiSetStyle(COMBOBOX, TEXT_COLOR_FOCUSED, focusedTextColor);

    GuiSetStyle(COMBOBOX, BORDER_COLOR_PRESSED, pressedBorderColor);
    GuiSetStyle(COMBOBOX, BASE_COLOR_PRESSED, pressedBaseColor);
    GuiSetStyle(COMBOBOX, TEXT_COLOR_PRESSED, pressedTextColor);

    GuiSetStyle(COMBOBOX, BORDER_COLOR_DISABLED, disabledBorderColor);
    GuiSetStyle(COMBOBOX, BASE_COLOR_DISABLED, disabledBaseColor);
    GuiSetStyle(COMBOBOX, TEXT_COLOR_DISABLED, disabledTextColor);

    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_NORMAL, defaultBorderColor);
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_NORMAL, defaultBaseColor); // background of dropdown
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_NORMAL, defaultTextColor);
    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_NORMAL, focusedBorderColor);

    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_FOCUSED, focusedBorderColor);
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_FOCUSED, focusedBaseColor);
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_FOCUSED, focusedTextColor);

    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_PRESSED, pressedBorderColor);
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_PRESSED, pressedBaseColor);
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_PRESSED, pressedTextColor);

    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_DISABLED, disabledBorderColor);
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_DISABLED, disabledBaseColor);
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_DISABLED, disabledTextColor);

    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(BLACK));
    GuiSetStyle(COMBOBOX, BACKGROUND_COLOR, ColorToInt(BLACK));
    GuiSetStyle(DROPDOWNBOX, BACKGROUND_COLOR, ColorToInt(BLACK));
    GuiSetStyle(LISTVIEW, BACKGROUND_COLOR, ColorToInt(BLACK));
    GuiSetStyle(STATUSBAR, BASE_COLOR_NORMAL, ColorToInt(BLACK));  // status bar background
    GuiSetStyle(STATUSBAR, TEXT_COLOR_NORMAL, ColorToInt(WHITE));  // status bar text
    GuiSetStyle(STATUSBAR, BORDER_COLOR_NORMAL, ColorToInt(GRAY)); // status bar outline

    /*
        GuiSetStyle(DEFAULT, LINE_COLOR, 0x663333ff);
        GuiSetStyle(COMBOBOX, LINE_COLOR, 0x663333ff);
        GuiSetStyle(DROPDOWNBOX, LINE_COLOR, 0x663333ff);
        GuiSetStyle(LISTVIEW, LINE_COLOR, 0x663333ff);
    */
}

void Overlay::input()
{
    // handle input for the overlay, e.g. for a console or debug menu
    if (IsKeyPressed(KEY_F9))
    {
        console = !console; // toggle console on/off
    }
}