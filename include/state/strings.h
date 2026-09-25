#pragma once

#include <cstring>
#include "raylib.h" // for TraceLog

inline void copyFixed(char *dst, size_t dstSize, const char *src)
{
    if (dstSize == 0)
        return;
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    if (std::strlen(src) >= dstSize)
    {
        TraceLog(LOG_WARNING, "copyFixed: truncated '%s' to %zu chars", src, dstSize - 1);
    }
    std::strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}