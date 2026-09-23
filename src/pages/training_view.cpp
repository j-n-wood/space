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
const Rectangle remove_engineers_button{815, 650, 36, 60};
const Rectangle add_engineers_button{880, 650, 36, 60};
const Rectangle remove_marines_button{1069, 650, 36, 60};
const Rectangle add_marines_button{1134, 650, 36, 60};

const float door_animation_duration = 0.5f; // seconds
const float door_animation_closed = 1.0f;   // closed position
const float door_animation_open = 0.0f;     // open position

void TrainingView::setTweenState(CrewType type)
{
    Crew *crew = facility->crewForType(type);

    if (crew && crew->inTraining())
    {
        door_tween[static_cast<int>(type)].reset(door_animation_open, door_animation_closed, 0.0f); // closed
    }
    else
    {
        door_tween[static_cast<int>(type)].reset(door_animation_closed, door_animation_open, 0.0f); // open
    }
}

void TrainingView::activate(ViewState &viewState)
{
    facility = viewState.getCurrentTrainingFacility();
    earthCity = Game::getCurrent()->earthCity();

    // explicitly set tween states
    setTweenState(CrewType::Scientist);
    setTweenState(CrewType::Engineer);
    setTweenState(CrewType::Marine);
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
    renderTrainingControls(CrewType::Scientist, remove_science_button, add_science_button);
    renderTrainingControls(CrewType::Engineer, remove_engineers_button, add_engineers_button);
    renderTrainingControls(CrewType::Marine, remove_marines_button, add_marines_button);
}

void TrainingView::renderTrainingControls(CrewType type, const Rectangle &remove_button, const Rectangle &add_button)
{
    auto &overlay = Overlay::getInstance();

    Crew *crew = facility->crewForType(type);

    Tween &tween = door_tween[static_cast<int>(type)];

    if (!crew || (!crew->inTraining()))
    {
        // door state should be open - set tween to open if not already
        if (tween.end_value >= door_animation_closed)
        {
            // animation closed, so reset to open
            tween.reset(door_animation_closed, door_animation_open, door_animation_duration);
        }

        if (overlay.mouseDownArea(remove_button, "Remove trainees", &repeat_button_state))
        {
            int returned = facility->removeTrainees(type, repeat_button_state.rate());
            earthCity->population += returned; // return to population pool
        }
        if (overlay.mouseDownArea(add_button, "Add trainees", &repeat_button_state))
        {
            int to_add = std::min(repeat_button_state.rate(), earthCity->population);
            int added = facility->addTrainees(type, to_add);
            earthCity->population -= added; // remove from population pool
        }
    }

    if (crew && crew->inTraining())
    {
        // close door over control area
        // use tween to animate door closing and opening

        // door state should be closed - set tween to close if not already
        if (tween.end_value <= door_animation_open)
        {
            // animation open, so reset to closed
            tween.reset(door_animation_open, door_animation_closed, door_animation_duration);
        }
    }

    auto value{tween.value()};
    if (value > door_animation_open)
    {
        // draw door over control area
        // TODO
        // dump text placeholder for now
        char buf[64];
        std::snprintf(buf, sizeof buf, "Door %.2f", value);
        DrawText(buf, remove_button.x + 10, remove_button.y - 30, 20, WHITE);
    }
}

void TrainingView::update(const float delta)
{
    repeat_button_state.update(delta);

    // once game time advances, a training crew cannot be altered
    // this can be detected without data change - a rank 0 crew with experience > 0 is in training

    for (int i = 0; i < static_cast<int>(CrewType::MAX_CREW_TYPE); ++i)
    {
        Tween &tween = door_tween[i];
        tween.update(delta);
    }
}
