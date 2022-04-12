#pragma once

// general information
// ===================
// - We don't delete pointers in our buffer, but just rely on boundaries
// - Memory barriers are not used because atomic reads per default use load(memory_order_seq_cst)

// - atomic compare exchange
//      bool r = x.compare_exchange_*(&expected, T desired)
//   is the same as
//      bool r = atomic_compare_exchange_*(atomic<T>*x, *expected, T desired)
//
//   compares x with expected (value):
//      if equal -> x becomes desired and return true
//      else -> expected is updated with actual value


#include "job.h"

#ifdef EXTRA_LOCKS
    #include <mutex>
#endif

class LocklessDeque
{
private:
    Job** mEntries;
    std::atomic_int_fast32_t mFront{ 0 }, mBack{ 0 };

    // just using a fixed size large enough for our testing setup
    // TODO: would need to allocate and provide space from worker thread
    static const int AMOUNT = 16;

#ifdef EXTRA_LOCKS
    using lock_guard = std::lock_guard<std::mutex>;
    mutable std::mutex mJobDequeMutex;
#endif

public:
    LocklessDeque()
    {
        size_t bufferSize = AMOUNT * sizeof(Job*);
        void* buffer = (void*)malloc(bufferSize);
        mEntries = (Job**)buffer;
    }

    ~LocklessDeque()
    {
        free(mEntries);
    }

    size_t Size() const
    {
        return mBack - mFront;
    }

    size_t AvailableJobs() const
    {
#ifdef EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        size_t jobs = 0;
        //size_t size = m_back - m_front;
        //for (size_t i = mFront; i < size; ++i)
        for (size_t i = mFront; i < mBack; ++i)
        {
            if (mEntries[i]->CanExecute())
            {
                jobs++;
            }
        }
        return jobs;
    }

    void Clear()
    {
        mBack = 0;
        mFront = 0;
    }

    void PushBack(Job* job)
    {
#ifdef EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        int_fast32_t jobIndex = mBack;
        mEntries[jobIndex] = job;
        mBack.fetch_add(1);
        HTL_LOGT(ThreadId, "=> pushed_back job " << job->GetName() << " for size: " << (mBack - mFront) <<
            ", front: " << mFront <<
            ", back: " << mBack <<
            ", index: " << (jobIndex));
    }

    // pull from private FIFO end, allows to return not executable job for reordering
    Job* PopFront(bool allowOpenDependencies = false)
    {
#ifdef EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        int_fast32_t front = mFront;
        int_fast32_t back = mBack;
        if (front < back)
        {
            Job* job = mEntries[front];
            if (allowOpenDependencies || job->CanExecute())
            {
                if (!mFront.compare_exchange_strong(front, front + 1))
                {
                    // concurrent thread changed front -> should not happen
                    HTL_LOGT(ThreadId, "COULD NOT POP FRONT");
                    return nullptr;
                }

                HTL_LOGT(ThreadId, "<= pop_front job size: " << (mBack - mFront) <<
                    ", front: " << mFront <<
                    ", back: " << mBack);

                // reset boundaries after popping last element
                if (mFront == mBack && front != 0)
                {
                    HTL_LOGTW(ThreadId, "POPPING FRONT last element, resetting boundaries: " << mFront << " - " << mBack);
                    mBack = 0;
                    mFront = 0;
                }
                return job;
            }
        }
        return nullptr; // queue empty or job not valid
    }

    Job* PopBack()
    {
#ifdef EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        int_fast32_t back = mBack;
        int_fast32_t front = mFront;
        if (front < back)
        {
            Job* job = mEntries[back - 1];
            // do not return a job if cannot execute
            if (!job->CanExecute())
            {
                return nullptr;
            }

            if (!mBack.compare_exchange_strong(back, mBack - 1))
            {
                // concurrent thread changed back -> main_looper called PushBack
                HTL_LOGT(ThreadId, "COULD NOT POP BACK -> BACK changed");
                return nullptr;
            }

            HTL_LOGT(ThreadId, "<= pop_back job size: " << (mBack - mFront) <<
                ", front: " << mFront <<
                ", back: " << mBack);

            // reset boundaries after popping last element
            if (mFront == mBack && front != 0)
            {
                HTL_LOGTW(ThreadId, "POPPING FRONT last element, resetting boundaries: " << mFront << " - " << mBack);
                mBack = 0;
                mFront = 0;
            }
            return job;
        }
        return nullptr; // queue already empty
    }

    // Debug functionality for printing additional information
    // should get stripped away by compiler if not used
    uint32_t ThreadId{ 0 };

    void Print() const
    {
#ifdef EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        HTL_LOGT(ThreadId, " -> current deque (" << (mBack - mFront) << "): ");
        for (size_t i = mFront; i < mBack; ++i)
        {
            HTL_LOGT(ThreadId, "\t" << mEntries[i]->GetName() << " - " << mEntries[i]->GetUnfinishedJobs());
        }
    }
};