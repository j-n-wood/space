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

bool Overlay::mouseDownArea(const Rectangle &area, const char *toolTip, RepeatButtonState *state)
{
    if (CheckCollisionPointRec(GetMousePosition(), area))
    {
        auto mb{IsMouseButtonDown(MOUSE_LEFT_BUTTON)};
        setCurrentToolTip(toolTip);
        DrawRectangleLinesEx(area, 1.0f, mb ? WHITE : GRAY);
        if (mb)
        {
            if (state->last_button != &area)
            {
                state->reset();
                state->last_button = &area; // set the last button after reset
                state->dead_time = 0.5f;    // initial dead time before repeat starts
                return true;
            }

            // same button
            state->held = true;
            if (state->dead_time > 0.0f)
            {
                return false;
            }

            return true;
        }
    }
    return false;
}

void RepeatButtonState::update(const float delta)
{
    if (IsMouseButtonUp(MOUSE_BUTTON_LEFT))
    {
        // reset count for add/remove
        reset();
    }
    else
    {
        if (held)
        {
            if (dead_time > 0.0f)
            {
                dead_time -= delta;
            }
            else
            {
                // increase add/remove rate if button is held down
                add_rate += delta * acceleration; // adjust multiplier for desired acceleration
            }
        }
    }
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

    // Following a craft needs no work here: ViewState reads the place through the craft,
    // so it tracks as the craft moves.

    // system | place | page, laid out by measuring rather than at fixed columns: place
    // names are now full ones like "Earth Orbital", so a fixed column would collide.
    // The place names its own side, so a page whose title would repeat it leaves the
    // title empty and contributes nothing here.
    {
        const int HEADER_TOP = 10;
        const int HEADER_SIZE = 20;
        const int HEADER_GAP = 20;
        int x = 10;

        auto drawField = [&](const char *text)
        {
            if (!text || !text[0])
            {
                return;
            }
            DrawText(text, x, HEADER_TOP, HEADER_SIZE, WHITE);
            x += MeasureText(text, HEADER_SIZE) + HEADER_GAP;
        };

        auto system = pm.viewState.getCurrentSystem();
        auto place = pm.viewState.getCurrentPlace();
        drawField(system ? system->name : nullptr);
        drawField(place ? place->name : nullptr);
        drawField(pm.getCurrentPage()->title);
    }

    if (toolTipSet)
    {
        auto mp = GetMousePosition();
        DrawText(currentToolTip, mp.x + 12, mp.y + 8, 20, WHITE);
    }

    // time
    char buf[256];
    sprintf(buf, "%.2f", game->game_time);
    DrawText(buf, BasePage::timeDest.x, BasePage::timeDest.y, 20, WHITE);

    // help text
    if (help)
    {
        const char *helpText = "Help:\n"
                               "F3: Master Control page\n"
                               "F5: Save game\n"
                               "F8: Load game\n"
                               "F10: Toggle debug tools\n"
                               "H: Toggle help text\n"
                               "Mouse: Interact with UI\n"
                               "Keyboard: Interact with UI\n"
                               "Console: Toggle with ~\n"
                               "Space: advance time\n"
                               "Tab: auto advance time\n"
                               "Time rate: +/-\n";
        DrawText(helpText, 10, 50, 20, WHITE);
    }

    // console
    if (console)
    {
        Rectangle consoleRect = {50, 950, 400, 50};
        GuiGroupBox(consoleRect, "Console");
        Rectangle inputRect = {consoleRect.x + 10, consoleRect.y + consoleRect.height - 40, consoleRect.width - 20, 30};
        GuiTextBox(inputRect, consoleInput, sizeof(consoleInput), true);
        if (IsKeyPressed(KEY_ENTER))
        {
            if (!game->processConsoleCommand(consoleInput, pm.viewState.getCurrentBody(), pm.viewState.getCurrentFacility()))
            {
                // some other commands - move to appropriate controller
                // command 'faction {faction_id}' to switch faction for testing
                int faction_id;
                if (std::sscanf(consoleInput, "faction %d", &faction_id) == 1)
                {
                    pm.viewState.setFactionId(faction_id);
                    pm.reactivateCurrentPage(); // refresh page to update any faction-specific info
                    TraceLog(LOG_INFO, "Switched to faction %d", faction_id);
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
    // Toggle the console on the backtick/tilde key (standard in-game console key).
    if (IsKeyPressed(KEY_GRAVE))
    {
        console = !console; // toggle console on/off
        // The grave key also enqueues its character (` / ~); drain the char queue so the
        // console text box (drawn later this frame) doesn't capture the toggle keystroke.
        while (GetCharPressed() != 0)
        {
        }
    }

    // Toggle debug tools (renderDebug overlays) on F10.
    if (IsKeyPressed(KEY_F10))
    {
        debug = !debug;
    }

    // Toggle help text from 'H'
    if (IsKeyPressed(KEY_H))
    {
        help = !help;
    }
}