#pragma once

#include "orrery.h"
#include "state/system.h"
#include "pages/pages.h"
#include <memory>

class DestinationPicker
{
public:
    System *system;
    OrreryPtr orrery;
    bool visible;

    DestinationPicker();

    static DestinationPicker *create(System *system, Vector2 center, float scale);

    void input();
    void render();

    void setCallbacks(void *caller, onDestinationSelected onSelected, onDestinationSelectCancelled onCancelled)
    {
        orrery->caller = caller;
        orrery->onDestinationSelectedCallback = onSelected;
        orrery->onDestinationSelectCancelledCallback = onCancelled;
    }
};

typedef std::unique_ptr<DestinationPicker> DestinationPickerPtr;