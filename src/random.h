#pragma once

#include <random>

class Random
{
public:
    Random();

    float Rand01();
    float Rand(float min, float max);

private:
    std::default_random_engine mRng;
    float mMax;
};