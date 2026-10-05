#pragma once

#include "state/realtime_event.h"

class SDMEvent : public FacilityRealtimeEvent
{
public:
    SDMEvent(double t, Facility *f) : FacilityRealtimeEvent{t, f} { type = RealtimeEventType::SDM_Active; };
    virtual ~SDMEvent() = default;
    bool cancelled() override;
    bool update(double delta) override;
    void onComplete() override;
};