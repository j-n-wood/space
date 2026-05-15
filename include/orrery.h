#pragma once

#include "state/system.h"
#include <memory>

class Orrery
{
    float last_time; // recalculate positions when time changes
    Vector2 body_positions[64];

    void updatePositions(float time);

    inline Vector2 renderPosition(const int index)
    {
        // apply scale and center to the raw body position
        // adjust by focus in system coordinates to allow for zooming in on a particular body (e.g. planet)

        return {
            this->center.x + (body_positions[index].x - focus.x) * this->scale,
            this->center.y + (body_positions[index].y - focus.y) * this->scale};
    }

    int mouseOverBodyIndex();

public:
    Vector2 center; // center of output render
    int focus_index;
    Vector2 focus; // point to focus on - zoom around this point rather than center (e.g. for zooming in on a planet). This is in solar coordinates.
    float scale;

    System *system;

    Orrery();

    ~Orrery()
    {
        // Note: we don't own the system pointer, so we won't free it here
    }

    void input();
    void render();

    Orrery &setSystem(System *s)
    {
        this->system = s;
        this->last_time = -1.0f; // force update of positions on next render
        return *this;
    }
};

typedef std::unique_ptr<Orrery> OrreryPtr;

OrreryPtr createOrrery(Vector2 center, float scale);