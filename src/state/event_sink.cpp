#include "state/event_sink.h"

extern "C"
{
#include "raylib.h"
}

void EventSink::addLog(const char *log)
{
    // default implementation does nothing - can be overridden by derived classes to e.g. log to UI
    (void)log; // suppress unused parameter warning
}

void EventSink::onOrbitalConstruction(Orbital *orbital)
{
    // default implementation does nothing - can be overridden by derived classes to react to orbital construction events
    (void)orbital; // suppress unused parameter warning
}

void EventSink::onResourceFacilityConstruction(ResourceFacility *rf)
{
    // default implementation does nothing - can be overridden by derived classes to react to resource facility construction events
    (void)rf; // suppress unused parameter warning
}

void EventSink::onProductionComplete(Factory *factory, int item_id)
{
    // default implementation does nothing - can be overridden by derived classes to react to production completion events
    (void)factory; // suppress unused parameter warning
    (void)item_id; // suppress unused parameter warning
}