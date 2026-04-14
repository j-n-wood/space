#pragma once

#include "base_page.h"
#include "orrery.h"

class SystemView : public BasePage
{
    std::unique_ptr<Orrery> orrery; // owned
public:
    explicit SystemView() : orrery(createOrrery((Vector2){640, 400}, 1.f))
    {
        backgroundSource = pageBackgroundSources[PB_COCKPIT];
        title = "System View";
    }
    ~SystemView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;
    void setSystem(System *s)
    {
        orrery->setSystem(s);
    }
};