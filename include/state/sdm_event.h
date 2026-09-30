#pragma once

#include "state/realtime_event.h"

class SDMEvent : public FacilityRealtimeEvent
{
public:
    SDMEvent(double t, Facility *f) : FacilityRealtimeEvent{t, f} {};
    virtual ~SDMEvent() = default;
    bool update(double delta) override;
    void onComplete() override;
};