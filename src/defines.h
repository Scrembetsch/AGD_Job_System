#pragma once

#include <stdint.h>
#include <stdio.h>

#define HTL_LOGE(...) printf("[ERROR]   In File '%s' at Line %d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n")
#define HTL_LOGW(...) printf("[WARNING] In File '%s' at Line %d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n")
#define HTL_LOGD(...) printf("[DEBUG]   In File '%s' at Line %d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n")

void static thread_log(uint32_t threadId, const char *msg)
{
    printf("\x1B[%dm%s on thread #%d\033[0m\n", threadId + 31, msg, threadId);
}

// custom defines for easier testing
// TODO: move together with macros from other files to defines.h file
//#define USING_LOCKLESS // using lockless variant of worker queue
//#define TEST_ONLY_ONE_FRAME // main loop returns after one execution
#define TEST_DEPENDENCIES // test if correct dependencies are met
//#define EXTRA_DEBUG // additional debug output

#ifdef _DEBUG
    #define EXTRA_DEBUG
#endif