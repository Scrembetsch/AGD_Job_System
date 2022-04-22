#pragma once

// general information
// ===================
// - We don't delete pointers in our buffer, but just rely on boundaries
// - Memory barriers are not used because atomic reads and writes per default use load(memory_order_seq_cst)
// - per design pop on private end should be LIFO and on public end FIFO
//      but I didn't pay attention at design time and changing it now would be a high risk breaking everything
//      -> would need to start boundaries at AMOUNT-1 instead of zero and add jobs per PushFront
//          but this way re-ordering (moving job from one end to the other because its dependencies are not fulfilled yet)
//          would not be possible unless starting boundaries in the middle of mEntries
//      -> alternatively and more secure would be to use a ringbuffer with generations to allow jumping over boundaries
//          or just creating nodes on heap (which we just assume would perform worse and we already know all our jobs)

// - atomic compare exchange
//      bool r = x.compare_exchange_*(&expected, T desired)
//   is the same as
//      bool r = atomic_compare_exchange_*(atomic<T>*x, *expected, T desired)
//
//   compares x with expected (value):
//      if equal -> x becomes desired and return true
//      else -> expected is updated with actual value and return false


#include "job.h"

#ifdef HTL_EXTRA_LOCKS
    #include <mutex>
#endif

class LocklessDeque
{
private:
    Job** mEntries;
    std::atomic_int_fast32_t mFront{ 0 }, mBack{ 0 };

    // (**)
    // just using a fixed size large enough for our testing setup
    // would need to allocate and provide space from worker thread
    static const int AMOUNT = 16;

#ifdef HTL_EXTRA_LOCKS
    using lock_guard = std::lock_guard<std::mutex>;
    mutable std::mutex mJobDequeMutex;
#endif

public:
    LocklessDeque()
    {
        // generational clash is the cause for malloc/free
        // mEntries = new Job*[AMOUNT];
        mEntries = (Job**)malloc(AMOUNT * sizeof(Job*));
    }

    ~LocklessDeque()
    {
        // delete[] mEntries;
        free(mEntries);
    }

    size_t Size() const
    {
        int_fast32_t back = mBack;
        int_fast32_t front = mFront;
        if (back <= front) return 0;

        return (static_cast<size_t>(back) - front);
    }

    bool HasExecutableJobs() const
    {
#ifdef HTL_EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        int_fast32_t back = mBack;
        int_fast32_t front = mFront;
        if (front < back)
        {
            for (size_t i = front; i < back; ++i)
            {
                if (mEntries[i]->CanExecute())
                {
                    return true;
                }
            }
        }
        return false;
    }

    void Clear()
    {
        mBack = 0;
        mFront = 0;
    }

    void PushBack(Job* job)
    {
#ifdef HTL_EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        // naive way of adding and assume we are fine...
        mEntries[mBack.fetch_add(1)] = job;
        HTL_LOGT(ThreadId, "=> Pushed_back job " << job->GetName() << ", front: " << mFront << ", back: " << mBack);
    }

    // pull from private FIFO end, allows to return not executable job for reordering
    Job* PopFront(bool allowOpenDependencies = false)
    {
#ifdef HTL_EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        int_fast32_t front = mFront;
        int_fast32_t back = mBack;
        if (front < back && back > 0)
        {
            Job* job = mEntries[front];
            if (allowOpenDependencies || job->CanExecute())
            {
                if (!mFront.compare_exchange_strong(front, front + 1))
                {
                    // concurrent thread changed front -> should not happen
                    HTL_LOGTW(ThreadId, "COULD NOT POP FRONT -> FRONT changed");
                    return nullptr;
                }

                HTL_LOGT(ThreadId, "<= Pop_front front: " << mFront << ", back: " << mBack);

                // reset boundaries after popping last element
                if (mFront == mBack && front != 0)
                {
                    mBack = 0;
                    mFront = 0;
                }
                return job;
            }
        }
        return nullptr; // queue empty or job not valid
    }

    // pull from public LIFO end (stealing) or when Front was not executable
    Job* PopBack()
    {
#ifdef HTL_EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        int_fast32_t back = mBack;
        int_fast32_t front = mFront;
        if (front < back && back > 0)
        {
            Job* job = mEntries[back - 1];
            // do not return a job if cannot execute
            if (!job->CanExecute())
            {
                return nullptr;
            }

            if (!mBack.compare_exchange_strong(back, back - 1))
            {
                // concurrent thread changed back -> main_looper called PushBack
                HTL_LOGTW(ThreadId, "COULD NOT POP BACK -> BACK changed");
                return nullptr;
            }

            HTL_LOGT(ThreadId, "<= Pop_back front: " << mFront << ", back: " << mBack);

            return job;
        }
        return nullptr; // queue already empty
    }

    // Debug functionality for printing additional information
    // should get stripped away by compiler if not used
    uint32_t ThreadId{ 0 };

    void Print() const
    {
#ifdef HTL_EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        int_fast32_t front = mFront;
        int_fast32_t back = mBack;
        HTL_LOG(" -> Current deque with boundaries: " << front << " - " << back << " (" << (back - front) << "): ");
        for (size_t i = front; i < back; ++i)
        {
            HTL_LOG("\t" << mEntries[i]->GetName() << " - " << mEntries[i]->GetUnfinishedJobs());
        }
    }
};