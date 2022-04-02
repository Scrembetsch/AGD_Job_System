#pragma once

#include "job.h"

#include <mutex>
//#include <queue>
#include <deque>

// could use template class but since we are only using our queue for jobs we don't need any additional overhead
//template <class T>
class LockingDeque
{
private:
    using lock_guard = std::lock_guard<std::mutex>;

    // TODO:
    // not sure if public getter need to set a mutext on queue?
    // because functions are const -> mutex needs to be mutable
    //      -> check what exactly this does
    mutable std::mutex mJobDequeMutex;
    std::deque<Job*> mJobDeque;

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

    // pull from private end
    Job* PopFront (bool ignoreContraints = false)
    {
        lock_guard lock(mJobDequeMutex);
        if (mJobDeque.empty()) return nullptr;

        if (ignoreContraints || mJobDeque.front()->CanExecute())
        {
            // combining Front and Pop in our implementation
            Job* job = mJobDeque.front();
            mJobDeque.pop_front();
            return job;
        }
        return nullptr;
    }

    // push to private end
    void PushFront(Job* job)
    {
        lock_guard lock(mJobDequeMutex);
        mJobDeque.push_front(job);
    }

    // pull from public end (stealing)
    Job* PopBack(bool ignoreContraints = false)
    {
        lock_guard lock(mJobDequeMutex);
        if (mJobDeque.empty()) return nullptr;

        if (ignoreContraints || mJobDeque.back()->CanExecute())
        {
            // combining Back and Pop in our implementation
            Job* job = mJobDeque.back();
            mJobDeque.pop_back();
            return job;
        }
        return nullptr;
    }

};