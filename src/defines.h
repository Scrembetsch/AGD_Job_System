#pragma once

#include "thread_safe_logger.h"

// custom defines for easier testing
#define HTL_USING_LOCKLESS // using lockless variant of worker queue
//#define HTL_EXTRA_LOCKS // still using locks in lockless queue for testing
#define HTL_TEST_DEPENDENCIES // test if correct dependencies are met
#define HTL_WAIT_FOR_AVAILABLE_JOBS // if set, conditional var wakes on executable jobs instead of size > 0
//#define HTL_TEST_ONLY_ONE_FRAME // main loop returns after one execution
//#define HTL_EXTRA_DEBUG // additional debug output
//#define HTL_SORT_JOBS // sort jobs to be allow workers to instantly start after pushing

// if compiling for debug mode enable additional output explicitly
#ifdef _DEBUG
    #define HTL_EXTRA_DEBUG
#endif

#define HTL_LOGE(message) ThreadSafeLogger::Logger << "\x1B[31m[ERROR]  " << message << "\033[0m\n"
#define HTL_LOGW(message) ThreadSafeLogger::Logger << "\x1B[33m[WARNING]" << message << "\033[0m\n"
#if defined(HTL_EXTRA_DEBUG)
    #define HTL_LOGD(message) ThreadSafeLogger::Logger << "[DEBUG]  " << message << "\n"
    #define HTL_LOGI(message) ThreadSafeLogger::Logger << "[INFO]   " << message << "\n"
#else
    #define HTL_LOGD(message)
    #define HTL_LOGI(message)
#endif
#define HTL_LOG(message) ThreadSafeLogger::Logger << message << "\n"

#define HTL_LOGTE(threadId, message) HTL_LOGE("\x1B[" << threadId + 31 << "m" << message << " on thread #" << threadId << "\033[0m")
#define HTL_LOGTW(threadId, message) HTL_LOGW("\x1B[" << threadId + 31 << "m" << message << " on thread #" << threadId << "\033[0m")
#if defined(HTL_EXTRA_DEBUG)
    #define HTL_LOGT(threadId, message) HTL_LOGI("\x1B[" << threadId + 31 << "m" << message << " on thread #" << threadId << "\033[0m")
#else
    #define HTL_LOGT(threadId, message)
#endif
