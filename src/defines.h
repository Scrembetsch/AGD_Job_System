#pragma once

#include "thread_safe_logger.h"

// custom defines for easier testing
#define USING_LOCKLESS // using lockless variant of worker queue
//#define TEST_ONLY_ONE_FRAME // main loop returns after one execution
#define TEST_DEPENDENCIES // test if correct dependencies are met
#define WAIT_FOR_AVAILABLE_JOBS // if set, conditional var wakes on executable jobs instead of size > 0
//#define EXTRA_DEBUG // additional debug output
//#define EXTRA_LOCKS // still using locks in lockless queue for testing
//#define SORT_JOBS // sort jobs to be allow workers to instantly start after pushing

//#ifdef _MSC_VER
//  #define HTL_COMPILER_BARRIER _ReadWriteBarrier()
//#endif
//#define HTL_MEMORY_BARRIER std::atomic_thread_fence(std::memory_order_seq_cst);

// if compiling for debug mode enable additional output explicitly
#ifdef _DEBUG
    #define EXTRA_DEBUG
#endif

#define HTL_LOGE(message) ThreadSafeLogger::Logger << "\x1B[31m[ERROR]  " << message << "\033[0m\n"
#define HTL_LOGW(message) ThreadSafeLogger::Logger << "\x1B[33m[WARNING]" << message << "\033[0m\n"
#if defined(EXTRA_DEBUG)
    #define HTL_LOGD(message) ThreadSafeLogger::Logger << "[DEBUG]  " << message << "\n"
    #define HTL_LOGI(message) ThreadSafeLogger::Logger << "[INFO]   " << message << "\n"
#else
    #define HTL_LOGD(message)
    #define HTL_LOGI(message)
#endif
#define HTL_LOG(message) ThreadSafeLogger::Logger << message << "\n"

#define HTL_LOGTE(threadId, message) HTL_LOGE("\x1B[" << threadId + 31 << "m" << message << " on thread #" << threadId << "\033[0m")
#define HTL_LOGTW(threadId, message) HTL_LOGW("\x1B[" << threadId + 31 << "m" << message << " on thread #" << threadId << "\033[0m")
#if defined(EXTRA_DEBUG)
    #define HTL_LOGT(threadId, message) HTL_LOGI("\x1B[" << threadId + 31 << "m" << message << " on thread #" << threadId << "\033[0m")
#else
    #define HTL_LOGT(threadId, message)
#endif
