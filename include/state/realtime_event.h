#pragma once

enum RealtimeEventType
{
    Empty_Event,
    SDM_Active,
    Craft_Under_Attack,
    Facility_Under_Attack
};

class RealtimeEvent
{
public:
    double time_remaining;
    RealtimeEventType type;

    RealtimeEvent(double t) : time_remaining{t}, type{Empty_Event} {};
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