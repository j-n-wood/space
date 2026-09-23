#include "pages/training_view.h"

#include "pages/research.h"
#include "state/game.h"
#include "assets/ui_elements.h"
#include "pages/overlay.h"

extern "C"
{
#include "raylib.h"
#include "raygui/raygui.h"
}

const Rectangle remove_science_button{556, 650, 36, 60};
const Rectangle add_science_button{620, 650, 36, 60};

void TrainingView::activate(ViewState &viewState)
{
    facility = viewState.getCurrentTrainingFacility();
    earthCity = Game::getCurrent()->earthCity();
}

void TrainingView::input()
{
}

void TrainingView::render()
{
    BasePage::render();

    auto &overlay = Overlay::getInstance();

    // population count
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d", earthCity->population);
    DrawText(buf, 228, 534, 20, WHITE);

    if (!facility)
    {
        return;
    }

    if (facility->scientists)
    {
        std::snprintf(buf, sizeof buf, "%d", facility->scientists->size);
        DrawText(buf, 556, 505, 20, WHITE);
    }
    if (facility->engineers)
    {
        std::snprintf(buf, sizeof buf, "%d", facility->engineers->size);
        DrawText(buf, 815, 505, 20, WHITE);
    }
    if (facility->marines)
    {
        std::snprintf(buf, sizeof buf, "%d", facility->marines->size);
        DrawText(buf, 1065, 505, 20, WHITE);
    }

    // controls using background image
    {

        if (overlay.mouseDownArea(remove_science_button, "Remove trainees", &repeat_button_state))
        {
            int returned = facility->removeTrainees(CrewType::Scientist, repeat_button_state.rate());
            earthCity->population += returned; // return to population pool
        }
        if (overlay.mouseDownArea(add_science_button, "Add trainees", &repeat_button_state))
        {
            int to_add = std::min(repeat_button_state.rate(), earthCity->population);
            int added = facility->addTrainees(CrewType::Scientist, to_add);
            earthCity->population -= added; // remove from population pool
        }
    }
}

void TrainingView::update(const float delta)
{
    repeat_button_state.update(delta);
}
