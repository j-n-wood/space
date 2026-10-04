#include "pages/comms_view.h"
#include "state/game.h"
#include "pages/overlay.h"
#include "pages/pages.h"
#include "assets/ui_elements.h"

CommsView::CommsView(int l, int t) : top(t), left(l), craft(nullptr), faction(nullptr), visible(false), state(FactionInteraction::Unset)
{
    state = FactionInteraction::Unset;
}

void CommsView::activate(Craft *c, Faction *f)
{
    craft = c;
    faction = f;
    // determine state from game state
    Game *game = Game::getCurrent();
    state = game->factionInteractionForCraft(craft, faction->id);

    visible = true;
    PageManager::getInstance().setModal(true);
}

void CommsView::deactivate()
{
    visible = false;
    PageManager::getInstance().setModal(false);
}

void CommsView::input()
{
    if (!visible)
    {
        return;
    }
}

void CommsView::render()
{
    if (!visible)
    {
        return;
    }

    ControlLockToggle lock(false); // unlock controls for this modal

    // add a dimmed background to make it clear this is an overlay
    DrawRectangle(left, top, GetScreenWidth() - 2 * left, GetScreenHeight() - 2 * top, (Color){0, 0, 0, 128});
    DrawRectangleLines(left, top, GetScreenWidth() - 2 * left, GetScreenHeight() - 2 * top, (Color){200, 180, 170, 255});

    auto &overlay = Overlay::getInstance();
    Rectangle yesButton = {left + 20.0f, top + 80.0f, 100.0f, 40.0f};
    Rectangle noButton = {left + 140.0f, top + 80.0f, 100.0f, 40.0f};

    switch (state)
    {
    case FactionInteraction::Unset:
    case FactionInteraction::Max:
        // render unknown state
        break;
    case FactionInteraction::DeclareWar:
        // render war state
        // does not have to be done at a craft
        DrawText("That's far enough Earthling.", left + 20, top + 40, 20, WHITE);

        if (overlay.renderButton(yesButton, "OK", "OK", GREEN))
        {
            deactivate();
        }
        break;
    case FactionInteraction::SympatheticWarning:
        // render sympathetic warning state
        DrawText("You are being deceived.", left + 20, top + 40, 20, WHITE);

        if (overlay.renderButton(yesButton, "OK", "Depart", GREEN))
        {
            // load fuzLazer object into pod 0
            // set pod type and content
            craft->setPodType(0, PodType::PT_TOOL);
            craft->pods[0].contentType = ItemType::Grapple;
            craft->pods[0].amount = 1;
            craft->pods[0].object = Game::getCurrent()->objectByID(2);
            craft->launch();
            deactivate();
        }

        // TODO: set hostility
        break;
    case FactionInteraction::Trade:
        // render trade state
        DrawText("Trade available.", left + 20, top + 40, 20, WHITE);
        // trade yes/no buttons
        if (overlay.renderButton(yesButton, "Yes", "Accept Trade", GREEN))
        {
            Game::getCurrent()->performTrade(craft);
            // auto launch
            craft->launch();
            deactivate();
        }
        if (overlay.renderButton(noButton, "No", "Decline Trade", RED))
        {
            craft->launch();
            deactivate();
        }
        break;
    case FactionInteraction::GiveCommspod:
        // text stating that commspod research object is given
        {
            int grapple_pod = craft->hasTool(ItemType::Grapple);
            if (grapple_pod > -1)
            {
                DrawText("Go research this.", left + 20, top + 40, 20, WHITE);
            }
            else
            {
                DrawText("You don't have a grapple pod.", left + 20, top + 40, 20, WHITE);
            }

            if (overlay.renderButton(yesButton, "OK", "Depart", GREEN))
            {
                // set grapple content to story object
                craft->pods[grapple_pod].object = Game::getCurrent()->objectByID(1);
                craft->launch();
                deactivate();
            }
        }
        break;
    case FactionInteraction::BeforeCommspod:
        // text asking for a grapple
        DrawText("We need a grapple to proceed.", left + 20, top + 40, 20, WHITE);

        if (overlay.renderButton(yesButton, "OK", "Depart", GREEN))
        {
            craft->launch();
            deactivate();
        }
        break;
    }
}