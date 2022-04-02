#pragma once

#include "job.h"

#include <queue>

// could use template class but since we are only using our queue for jobs we don't need any additional overhead
//template <class T>
class LocklessQueue
{
private:
    std::queue<Job*> mJobQueue;

public:
    bool IsEmpty() const
    {
        return mJobQueue.empty();
    }

    size_t Size() const
    {
        return mJobQueue.size();
    }

    Job* Front()
    {
        return mJobQueue.front();
    }

    void Pop ()
    {
        mJobQueue.pop();
    }

    void Push(Job* job)
    {
        mJobQueue.push(job);
    }
};