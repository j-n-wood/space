#include "pages/message_box.h"
#include "state/strings.h"
#include "pages/overlay.h"
#include "state/game.h"

MessageBox::MessageBox(int l, int t, const char *message) : Modal(l, t), event(nullptr), craft(nullptr), interactionType(MessageInteractionType::NONE)
{
    copyFixed(buf, sizeof(buf), message);
}

MessageBox::MessageBox(int l, int t, Event *e, Craft *c) : Modal(l, t), event(e), craft(c), interactionType(MessageInteractionType::NONE)
{
    copyFixed(buf, sizeof(buf), e->email_message);
    interactionType = e->interactionType;
}

void MessageBox::render()
{
    Modal::render();
    DrawText(buf, left + 20, top + 40, 20, WHITE);
    auto &overlay = Overlay::getInstance();
    switch (interactionType)
    {
    case MessageInteractionType::NONE:
        // OK button
        {
            Rectangle okButton = {left + 20.0f, top + 80.0f, 100.0f, 40.0f};
            if (overlay.renderButton(okButton, "OK", "OK", GREEN))
            {
                close(ModalResult::OK);
            }
        }
        break;
    case MessageInteractionType::YESNO:
    {
        Rectangle noButton = {left + 20.0f, top + 80.0f, 100.0f, 40.0f};
        if (overlay.renderButton(noButton, "No", "No", RED))
        {
            close(ModalResult::No);
        }
        // Yes button
        Rectangle yesButton = {left + 140.0f, top + 80.0f, 100.0f, 40.0f};
        if (overlay.renderButton(yesButton, "Yes", "Yes", GREEN))
        {
            close(ModalResult::Yes);
        }
    }
    break;
    }
}

void MessageBox::onClose()
{
    // default event handling - if an OK-class result, complete the related game event
    bool proceed = (result == ModalResult::OK || result == ModalResult::Yes);
    if (event)
    {
        Game::getCurrent()->applyEvent(event, craft, proceed); // include context parameter
    }
}
