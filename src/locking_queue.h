#pragma once

#include "job.h"

#include <mutex>
//#include <queue>
#include <deque>

// could use template class but since we are only using our queue for jobs we don't need any additional overhead
// template <class T>
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
    // Get mutex to lock access outside of this class, otherwise lock is sometimes released to early
    std::mutex& GetMutex() const
    {
        return mJobDequeMutex;
    }

    bool IsEmpty() const
    {
        return mJobDeque.empty();
    }

    size_t Size() const
    {
        return mJobDeque.size();
    }

    Job* Front()
    {
        if (mJobDeque.empty()) return nullptr;

        return mJobDeque.front();
    }

    // pull from private end
    Job* PopFront ()
    {
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
        mJobDeque.push_front(job);
    }

    // pull from public end (stealing)
    Job* PopBack()
    {
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