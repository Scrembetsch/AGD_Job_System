#pragma once

#include <stdint.h>
#include <stdio.h>

#include "thread_safe_logger.h"

#ifdef _DEBUG
    #define EXTRA_DEBUG
#endif

#define HTL_LOGE(message) ThreadSafeLogger::Logger << "[ERROR]  " << message << "\n";
#define HTL_LOGW(message) ThreadSafeLogger::Logger << "[WARNING]"
#if defined(_DEBUG) || defined(EXTRA_DEBUG)
#define HTL_LOGD(message) ThreadSafeLogger::Logger << "[DEBUG]  " << message << "\n";
#else
#define HTL_LOGD(message) ;
#endif
#if defined(EXTRA_DEBUG)
#define HTL_LOGI(message) ThreadSafeLogger::Logger << "[INFO]   " << message << "\n";
#else
#define HTL_LOGI(message) ;
#endif
#define HTL_LOGTE(threadId, message) HTL_LOGE("\x1B[" << threadId + 31 << "m" << message << " on thread # " << threadId << "\033[0m");
#if defined(EXTRA_DEBUG)
#define HTL_LOGT(threadId, message) HTL_LOGI("\x1B[" << threadId + 31 << "m" << message << " on thread # " << threadId << "\033[0m");
#else
#define HTL_LOGT(threadId, message) ;
#endif

#define HTL_LOG(message) ThreadSafeLogger::Logger << message << "\n";

// custom defines for easier testing
// TODO: move together with macros from other files to defines.h file
//#define USING_LOCKLESS // using lockless variant of worker queue
//#define TEST_ONLY_ONE_FRAME // main loop returns after one execution
#define TEST_DEPENDENCIES // test if correct dependencies are met
//#define EXTRA_DEBUG // additional debug output
