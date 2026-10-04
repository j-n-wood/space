#include "pages/message_box.h"
#include "state/strings.h"
#include "pages/overlay.h"

MessageBox::MessageBox(int l, int t, const char *message) : Modal(l, t)
{
    copyFixed(buf, sizeof(buf), message);
}

void MessageBox::render()
{
    if (!visible)
    {
        return;
    }

    Modal::render();
    DrawText(buf, left + 20, top + 40, 20, WHITE);

    // OK button
    Rectangle okButton = {left + 20.0f, top + 80.0f, 100.0f, 40.0f};
    auto &overlay = Overlay::getInstance();
    if (overlay.renderButton(okButton, "OK", "OK", GREEN))
    {
        deactivate();
    }
}