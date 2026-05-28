#pragma once

#include "state/event_sink.h"

const int log_history_size = 4;
const float log_display_duration = 1.2; // seconds

class PageLog : public EventSink
{
    void shiftLogsUp();

public:
    int top;
    int left;
    float timeSinceLastLog;

    char logBuffer[log_history_size][256];

    PageLog();
    virtual void addLog(const char *log) override;
    void render();
    void update(const float delta);
};