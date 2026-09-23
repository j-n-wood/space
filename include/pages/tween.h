#pragma once

class Tween
{
public:
    float start_value{0.0f};
    float end_value{1.0f};
    float timer{0.0f};
    float duration{1.0f};
    bool active{false};

    inline void reset(float start = 0.0f, float end = 1.0f, float dur = 1.0f)
    {
        start_value = start;
        end_value = end;
        duration = dur;
        timer = 0.0f;
        active = (duration > 0.0f);
    }

    inline void start(float d = -1.0f) // use -1.0f to keep current duration
    {
        timer = 0.0f;
        if (d >= 0.0f)
        {
            duration = d;
        }
        active = (duration > 0.0f);
    }
    bool update(float delta); // return true if complete
    float progress() const;
    float value() const;
};