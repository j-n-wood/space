#include "pages/page_log.h"
#include "raylib.h"
#include <cstdio>

PageLog::PageLog() : top(0), left(0), timeSinceLastLog(0.0f)
{
    // initialize log buffer to empty strings
    for (int i = 0; i < log_history_size; i++)
    {
        logBuffer[i][0] = '\0';
    }
}

void PageLog::addLog(const char *log)
{
    // shift existing logs up
    shiftLogsUp();
    // add new log at the bottom
    std::snprintf(logBuffer[0], 256, "%s", log);
    timeSinceLastLog = 0.0f;
}

void PageLog::render()
{
    // render log entries with the most recent at the bottom, and older entries above it
    // fade out older entries with alpha

    Color text_colour = WHITE;
    for (int i = 0; i < log_history_size; i++)
    {
        unsigned char fade_factor = 255 - (i * 255 / log_history_size);
        text_colour.r = text_colour.b = fade_factor;
        text_colour.a = fade_factor;
        DrawText(logBuffer[i], left, top - i * 20 - 10, 20, text_colour);
    }
}

void PageLog::shiftLogsUp()
{
    // shift existing logs up
    for (int i = log_history_size - 1; i > 0; i--)
    {
        std::snprintf(logBuffer[i], 256, "%s", logBuffer[i - 1]);
    }
    // clear the oldest log
    logBuffer[0][0] = '\0';
}

void PageLog::update(const float delta)
{
    // automatically fade out logs over time by shifting them up and clearing the oldest one after a certain duration
    timeSinceLastLog += delta;
    if (timeSinceLastLog > log_display_duration)
    {
        // shift logs up
        shiftLogsUp();
        timeSinceLastLog = 0.0f;
    }
}