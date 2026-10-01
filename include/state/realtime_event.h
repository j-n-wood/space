#pragma once

class RealtimeEvent
{
public:
    double time_remaining;

    RealtimeEvent(double t) : time_remaining{t} {};
    virtual ~RealtimeEvent() = default;
    virtual bool cancelled() = 0;          // true -> event was cancelled
    virtual bool update(double delta) = 0; // true -> still active
    virtual void onComplete() = 0;
};

class Craft;
class Facility;

class CraftRealtimeEvent : public RealtimeEvent
{
public:
    Craft *craft;

    CraftRealtimeEvent(double t, Craft *c) : RealtimeEvent{t}, craft{c} {};
    virtual ~CraftRealtimeEvent() = default;
};

class FacilityRealtimeEvent : public RealtimeEvent
{
public:
    Facility *facility;

    FacilityRealtimeEvent(double t, Facility *f) : RealtimeEvent{t}, facility{f} {};
    virtual ~FacilityRealtimeEvent() = default;
};