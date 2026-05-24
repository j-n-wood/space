#pragma once

#include "state/log_sink.h"

const int log_history_size = 4;
const int log_display_duration = 1.2; // seconds

class PageLog : public LogSink
{
    void shiftLogsUp();

public:
    int top;
    int left;
    float timeSinceLastLog;

    char logBuffer[log_history_size][256];

    PageLog();
    void addLog(const char *log);
    void render();
    void update(const float delta);
};