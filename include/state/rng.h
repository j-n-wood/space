#pragma once

#include <random>
class Rng
{
    std::mt19937 engine;
    uint32_t seedValue{1};
    uint64_t draws{0};

public:
    void seed(uint32_t s)
    {
        seedValue = s;
        engine.seed(s);
        draws = 0;
    }
    uint32_t next()
    {
        ++draws;
        return engine();
    }
    int range(int lo, int hi) { return lo + int(next() % uint32_t(hi - lo + 1)); } // inclusive
    float unit() { return (next() >> 8) * (1.0f / 16777216.0f); }                  // [0, 1)
    uint32_t getSeed() const { return seedValue; }
    uint64_t getDraws() const { return draws; }
    void restore(uint32_t s, uint64_t n)
    {
        seed(s);
        engine.discard(n);
        draws = n;
    }
};