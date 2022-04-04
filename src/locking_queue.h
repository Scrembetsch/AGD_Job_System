#pragma once

#include "job.h"

#include <mutex>
#include <deque>

// could use template class but since we are only using our queue for jobs we don't need any additional overhead
// template <class T>
class LockingDeque
{
private:
    // TODO: not sure if public getter need to set a mutext on queue?
    using lock_guard = std::lock_guard<std::mutex>;

    // mutex needs do be mutable to be used in const functions
    mutable std::mutex mJobDequeMutex;
    std::deque<Job*> mJobDeque;

    // TODO: maybe use an atomic for size to check empty without locks?

public:
    bool IsEmpty() const
    {
        lock_guard lock(mJobDequeMutex);
        return mJobDeque.empty();
    }

    size_t Size() const
    {
        lock_guard lock(mJobDequeMutex);
        return mJobDeque.size();
    }

    Job* Front()
    {
        lock_guard lock(mJobDequeMutex);
        if (mJobDeque.empty()) return nullptr;

        return mJobDeque.front();
    }

    // pull from private end
    Job* PopFront ()
    {
        lock_guard lock(mJobDequeMutex);
        if (mJobDeque.empty()) return nullptr;

        if (mJobDeque.front()->CanExecute())
        {
            // combining front and pop in our implementation
            Job* job = mJobDeque.front();
            mJobDeque.pop_front();
            return job;
        }
        return nullptr;
    }

    void PushFront(Job* job)
    {
        lock_guard lock(mJobDequeMutex);
        mJobDeque.push_front(job);
    }

    // pull from public end (stealing)
    Job* PopBack()
    {
        lock_guard lock(mJobDequeMutex);
        if (mJobDeque.empty()) return nullptr;

        if (mJobDeque.back()->CanExecute())
        {
            // combining back and pop in our implementation
            Job* job = mJobDeque.back();
            mJobDeque.pop_back();
            return job;
        }
        return nullptr;
    }
};