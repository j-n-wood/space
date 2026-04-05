#pragma once

#include "wrappers/texture.h"

typedef enum {
    UI_EARTH_CITY_CONTROLS,
    UI_CONTROLS,
    UI_ELEMENT_COUNT
} UIElementID;

const Rectangle uiElementSources[UI_ELEMENT_COUNT] = {
    {174, 23, 51, 120},
    {230, 23, 51, 120},
};