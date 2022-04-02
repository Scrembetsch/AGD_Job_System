#pragma once

#include <iostream>
#include <sstream>
#include <mutex>

using namespace std;

class ThreadSafeLogger
{
public:
    template <typename T>
    static void log(T& message)
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << message.str();
        message.flush();
    }
    static ThreadSafeLogger Logger;
private:
    static std::mutex mutex;
};

struct LogBuffer {
    stringstream ss;

    LogBuffer() = default;
    LogBuffer(const LogBuffer&) = delete;
    LogBuffer& operator=(const LogBuffer&) = delete;
    LogBuffer& operator=(LogBuffer&&) = delete;
    LogBuffer(LogBuffer&& buf) : ss(move(buf.ss)) {
    }
    template <typename T>
    LogBuffer& operator<<(T&& message)
    {
        ss << std::forward<T>(message);
        return *this;
    }

    ~LogBuffer() {
        ThreadSafeLogger::log(ss);
    }
};

template <typename T>
LogBuffer operator<<(ThreadSafeLogger& simpleLogger, T&& message)
{
    LogBuffer buf;
    buf.ss << std::forward<T>(message);
    return buf;
}
