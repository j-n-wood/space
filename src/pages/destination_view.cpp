#include "pages/destination_view.h"

DestinationPicker::DestinationPicker() : system(nullptr), orrery(nullptr), visible(false)
{
}

DestinationPicker *DestinationPicker::create(System *system, Vector2 center, float scale)
{
    DestinationPicker *picker = new DestinationPicker();
    picker->system = system;
    picker->orrery = createOrrery(center, scale);
    picker->orrery->setSystem(system);
    return picker;
}

void DestinationPicker::input()
{
    if (!visible)
        return;
    orrery->input();
}

void DestinationPicker::render()
{
    if (!visible)
    {
        return;
    }
    orrery->render();
}