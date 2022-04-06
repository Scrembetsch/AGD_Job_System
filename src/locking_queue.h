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
    bool IsEmpty() const
    {
        return mSize == 0;
    }

    size_t Size()
    {
        return mSize;
    }

    Job* Front()
    {
        lock_guard lock(mJobDequeMutex);
        if (mSize == 0) return nullptr;

        return mJobDeque.front();
    }

    // pull from private end
    Job* PopFront ()
    {
        lock_guard lock(mJobDequeMutex);
        if (mSize == 0) return nullptr;

        if (mJobDeque.front()->CanExecute())
        {
            // combining front and pop in our implementation
            Job* job = mJobDeque.front();
            mJobDeque.pop_front();
            DecrementSize();
            return job;
        }
        return nullptr;
    }

    void PushFront(Job* job)
    {
        lock_guard lock(mJobDequeMutex);
        mJobDeque.push_front(job);
        IncrementSize();
    }

    // pull from public end (stealing)
    Job* PopBack()
    {
        lock_guard lock(mJobDequeMutex);
        if (mSize == 0) return nullptr;

        if (mJobDeque.back()->CanExecute())
        {
            // combining back and pop in our implementation
            Job* job = mJobDeque.back();
            mJobDeque.pop_back();
            DecrementSize();
            return job;
        }
        return nullptr;
    }

    // move front to back and retrieve the next front job
    Job* HireBack()
    {
        lock_guard lock(mJobDequeMutex);
        if (mSize < 2) return nullptr;

        Job* job = mJobDeque.front();
        mJobDeque.push_back(job);
        mJobDeque.pop_front();

        if (mJobDeque.front()->CanExecute())
        {
            // combining front and pop in our implementation
            job = mJobDeque.front();
            mJobDeque.pop_front();
            DecrementSize();
            return job;
        }
        return nullptr;
    }

    void Print()
    {
        lock_guard lock(mJobDequeMutex);
        std::cout << " -> current deque (" << mJobDeque.size() << "): ";
        for (size_t i = 0; i < mJobDeque.size(); ++i)
        {
            std::cout << mJobDeque[i]->GetName() << " - " << mJobDeque[i]->GetUnfinishedJobs() << ", ";
        }
        std::cout << std::endl;
    }

private:
    void IncrementSize()
    {
        // used for testing bug, where thread would go sleeping even if there would be a job
        //std::thread([this] {
        //    std::this_thread::sleep_for(std::chrono::nanoseconds(10));
        //    mSize++;
        //    }).detach();
        mSize++;
    }

    void DecrementSize()
    {
        if (mSize == 0)
        {
            std::cout << " oh noe size would be decremented below 0 :-o";
        }
        mSize--;
    }
};