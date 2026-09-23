#include "pages/tween.h"

bool Tween::update(float delta)
{
    if (!active)
    {
        return false;
    }
    timer += delta;
    if (timer > duration)
    {
        timer = duration;
        return true;
    }
    return false;
}

float Tween::progress() const
{
    float t = timer / duration;
    if (t < 0.0f)
    {
        t = 0.0f;
    }
    else if (t > 1.0f)
    {
        t = 1.0f;
    }
    return t;
}

float Tween::value() const
{
    if (duration <= 0.0f)
    {
        return end_value;
    }
    return start_value + (end_value - start_value) * progress();
}