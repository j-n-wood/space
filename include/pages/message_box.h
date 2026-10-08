#pragma once
#include "pages/modal.h"
#include "state/event.h"

class Craft;

class MessageBox : public Modal
{
protected:
    char buf[256]; // storage of message s.t. composed messages do not vanish

public:
    // context properties
    Event *event; // associated event for the message box, if any
    Craft *craft; // associated craft for the message box, if any

    MessageInteractionType interactionType; // type of interaction for the message box, if any

    MessageBox(int l, int t, const char *message);
    MessageBox(int l, int t, Event *e, Craft *c);

    virtual void render() override;
    virtual void onClose() override;
};