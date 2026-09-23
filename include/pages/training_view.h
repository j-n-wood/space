#pragma once

#include <cstdio>

#include "state/training_facility.h"
#include "state/earth_city.h"
#include "base_page.h"
#include "pages/overlay.h"
#include "pages/tween.h"

class TrainingView : public BasePage
{

    TrainingFacility *facility;
    EarthCity *earthCity;

    // UI state
    RepeatButtonState repeat_button_state;
    Tween door_tween[3];

    void setTweenState(CrewType type);
    void renderTrainingControls(CrewType type, const Rectangle &remove_button, const Rectangle &add_button);

public:
    TrainingView() : facility{nullptr}, earthCity{nullptr}, repeat_button_state{0.3f}
    {
        backgroundSource = pageBackgroundSources[PB_TRAINING];
        std::snprintf(title, sizeof title, "Training");
    }
    ~TrainingView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;
    void update(const float delta) override;
};