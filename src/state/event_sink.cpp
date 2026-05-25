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