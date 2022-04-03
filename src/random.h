#pragma once
#include <stdint.h>

class Random
{
public:
    Random();

    uint32_t Rand(uint32_t min, uint32_t max);
};