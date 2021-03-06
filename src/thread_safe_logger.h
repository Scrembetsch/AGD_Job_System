#pragma once

#include <iostream>
#include <sstream>
#include <mutex>

class ThreadSafeLogger
{
public:
    template <typename T>
    static void log(T& message)
    {
        // somehow this also works without the mutex, but we want to be safe
        std::lock_guard<std::mutex> lock(mMutex);
        std::cout << message.str();
        message.flush();
    }
    static ThreadSafeLogger Logger;

private:
    static std::mutex mMutex;
};

struct LogBuffer
{
    std::stringstream ss;

    LogBuffer() = default;
    LogBuffer(const LogBuffer&) = delete;
    LogBuffer& operator=(const LogBuffer&) = delete;
    LogBuffer& operator=(LogBuffer&&) = delete;

    LogBuffer(LogBuffer&& buf) noexcept
        : ss(std::move(buf.ss))
    {
    }
    template <typename T>
    LogBuffer& operator<<(T&& message)
    {
        ss << std::forward<T>(message);
        return *this;
    }

    ~LogBuffer()
    {
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
