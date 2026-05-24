#pragma once

class LogSink
{
public:
    virtual void addLog(const char *log) = 0;
};

class NullLogSink : public LogSink
{
public:
    void addLog(const char *log) override
    {
        // do nothing
    }
};