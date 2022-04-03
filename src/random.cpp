#include "random.h"

#include <stdlib.h> // srand, rand
#include <time.h>   //time

Random::Random()
{
    srand((unsigned)time(0));
}

uint32_t Random::Rand(uint32_t min, uint32_t max)
{
    return rand() % max + min;
}