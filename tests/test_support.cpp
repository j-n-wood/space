// Provides raygui implementation for the test build.
// In the game binary, this lives in main.cpp which is excluded from tests.

#define RAYGUI_IMPLEMENTATION

extern "C"
{
#include "raylib.h"
#include "raygui/raygui.h"
}
