#pragma once

#include "orrery.h"

typedef struct {
    Orrery* orrery;
} SystemView;

SystemView* createSystemView(Orrery* orrery);

void destroySystemView(SystemView* view);

void RenderSystemView(SystemView* view);