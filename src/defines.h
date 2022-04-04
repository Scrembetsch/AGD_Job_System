#pragma once

#include "thread_safe_logger.h"

#ifdef _DEBUG
    #define EXTRA_DEBUG
#endif

// custom defines for easier testing
//#define USING_LOCKLESS // using lockless variant of worker queue
//#define TEST_ONLY_ONE_FRAME // main loop returns after one execution
#define TEST_DEPENDENCIES // test if correct dependencies are met
//#define EXTRA_DEBUG // additional debug output

#define HTL_LOGE(message) ThreadSafeLogger::Logger << "\x1B[31m[ERROR]  " << message << "\033[0m\n"
#define HTL_LOGW(message) ThreadSafeLogger::Logger << "\x1B[33m[WARNING]" << message << "\033[0m\n"
#if defined(EXTRA_DEBUG)
#define HTL_LOGD(message) ThreadSafeLogger::Logger << "[DEBUG]  " << message << "\n"
#else
#define HTL_LOGD(message) ;
#endif
#if defined(EXTRA_DEBUG)
#define HTL_LOGI(message) ThreadSafeLogger::Logger << "[INFO]   " << message << "\n"
#else
#define HTL_LOGI(message) ;
#endif
#define HTL_LOGTE(threadId, message) HTL_LOGE("\x1B[" << threadId + 31 << "m" << message << " on thread #" << threadId << "\033[0m")
#if defined(EXTRA_DEBUG)
#define HTL_LOGT(threadId, message) HTL_LOGI("\x1B[" << threadId + 31 << "m" << message << " on thread #" << threadId << "\033[0m")
#else
#define HTL_LOGT(threadId, message) ;
#endif

#define HTL_LOG(message) ThreadSafeLogger::Logger << message << "\n"