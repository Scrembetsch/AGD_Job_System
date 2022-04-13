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

    std::atomic_size_t mSize{ 0 };

public:
    size_t Size() const
    {
        lock_guard lock(mJobDequeMutex);
        return mSize;
    }

    size_t HasExecutableJobs() const
    {
        lock_guard lock(mJobDequeMutex);
        for (size_t i = 0; i < mSize; ++i)
        {
            if (mJobDeque[i]->CanExecute())
            {
                return true;
            }
        }
        return false;
    }

    void Clear()
    {
        lock_guard lock(mJobDequeMutex);
        while (mSize > 0)
        {
            mJobDeque.pop_front();
            mSize--;
        }
    }

    // default push filled from worker
    void PushFront(Job* job)
    {
        lock_guard lock(mJobDequeMutex);
        mJobDeque.push_front(job);
        mSize++;
    }

    // needed to re-append front job if depencencies are not met yet
    void PushBack(Job* job)
    {
        lock_guard lock(mJobDequeMutex);
        mJobDeque.push_back(job);
        mSize++;
    }

    // pull from private FIFO end, allows to return not executable job for reordering
    Job* PopFront (bool allowOpenDependencies = false)
    {
        lock_guard lock(mJobDequeMutex);
        if (mSize == 0) return nullptr;

        if (allowOpenDependencies || mJobDeque.front()->CanExecute())
        {
            // combining front and pop in our implementation
            Job* job = mJobDeque.front();
            mJobDeque.pop_front();
            mSize--;
            return job;
        }
        return nullptr;
    }

    // pull from public FIFO end (stealing)
    Job* PopBack()
    {
        lock_guard lock(mJobDequeMutex);
        if (mSize == 0) return nullptr;

        if (mJobDeque.back()->CanExecute())
        {
            // combining back and pop in our implementation
            Job* job = mJobDeque.back();
            mJobDeque.pop_back();
            mSize--;
            return job;
        }
        return nullptr;
    }

    // Debug functionality for printing additional information
    // should get stripped away by compiler if not used
    uint32_t ThreadId{ 0 };

    Job* Front() const
    {
        lock_guard lock(mJobDequeMutex);
        if (mSize == 0) return nullptr;

        return mJobDeque.front();
    }

    void Print() const
    {
        //lock_guard lock(mJobDequeMutex);
        /*HTL_LOGT(ThreadId, " -> Current deque (" << mJobDeque.size() << "): ");
        for (size_t i = 0; i < mJobDeque.size(); ++i)
        {
            HTL_LOGT(ThreadId, "\t" << mJobDeque[i]->GetName() << " - " << mJobDeque[i]->GetUnfinishedJobs());
        }*/

        HTL_LOG(" -> Current deque (" << mJobDeque.size() << "): ");
        for (size_t i = 0; i < mJobDeque.size(); ++i)
        {
            HTL_LOG("\t" << mJobDeque[i]->GetName() << " - " << mJobDeque[i]->GetUnfinishedJobs());
        }
    }
};