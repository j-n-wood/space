#pragma once

#include "state/faction.h"

class Orbital;
class ResourceFacility;
class Factory;
class Facility;
class Craft;

class EventSink
{
public:
    virtual void addLog(const char *log);

    virtual void onOrbitalConstruction(Orbital *orbital);
    virtual void onResourceFacilityConstruction(ResourceFacility *rf);
    virtual void onProductionComplete(Factory *factory, int item_id);

    virtual void onFacilityDestroyed(Facility *f);
    virtual void onCraftDestroyed(Craft *c);

    virtual void onFactionInteraction(Faction *faction, FactionInteraction interaction, Craft *craft);
};