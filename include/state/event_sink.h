#pragma once

class Orbital;
class ResourceFacility;
class Factory;

class EventSink
{
public:
    virtual void addLog(const char *log);

    virtual void onOrbitalConstruction(Orbital *orbital);
    virtual void onResourceFacilityConstruction(ResourceFacility *rf);
    virtual void onProductionComplete(Factory *factory, int item_id);
};