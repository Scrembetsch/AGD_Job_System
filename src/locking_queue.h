#pragma once

#include "job.h"

#include <mutex>
#include <queue>

// could use template class but since we are only using our queue for jobs we don't need any additional overhead
//template <class T>
class LockingQueue
{
private:
    using lock_guard = std::lock_guard<std::mutex>;

    // TODO:
    // not sure if public getter need to set a mutext on queue?
    // because functions are const -> mutex needs to be mutable
    //      -> check what exactly this does
    mutable std::mutex mJobQueueMutex;
    std::queue<Job*> mJobQueue;

public:
    bool IsEmpty() const
    {
        lock_guard lock(mJobQueueMutex);
        return mJobQueue.empty();
    }

    size_t Size() const
    {
        lock_guard lock(mJobQueueMutex);
        return mJobQueue.size();
    }

    Job* Front()
    {
        lock_guard lock(mJobQueueMutex);
        if (mJobQueue.empty()) return nullptr;

        return mJobQueue.front();
    }

    // combining Front and Pop in our implementation
    Job* Pop (bool ignoreContraints = false)
    {
        lock_guard lock(mJobQueueMutex);
        if (mJobQueue.empty()) return nullptr;

        if (ignoreContraints || mJobQueue.front()->CanExecute())
        {
            Job* job = mJobQueue.front();
            mJobQueue.pop();
            return job;
        }
        return nullptr;
    }

    void Push(Job* job)
    {
        lock_guard lock(mJobQueueMutex);
        mJobQueue.push(job);
    }
};