#pragma once

#include "job.h"

#ifdef EXTRA_LOCKS
    #include <mutex>
#endif


// TODO:
// - fix LIFO / FIFO for private/public end
// - uniform and check CAS handling
// - allocate enough memory from outside and provide, allow resize/shrink
// - fix workaround for resetting boundaries in const AllJobsFinished
// - fix shutdown from main thread
// - measure and put results in README.md

// Status
// Debug with 2 threads seems to run fine :D
// Debug with 7 threads crashes after a while
// Release with 2 threads crashes
// Release with 7 threads crashes
// --> there is defenitely a bug with resetting boundaries

// general information
// - we don't delete pointers in our buffer, but just rely on boundaries
// - memory barriers are not used because atomic reads per default use load(memory_order_seq_cst)

class LocklessDeque
{
private:
    Job** m_entries;
    std::atomic_int_fast32_t m_front{ 0 }, m_back{ 0 };

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
        m_entries = (Job**)buffer;
    }

    ~LocklessDeque()
    {
        free(m_entries);
    }

    size_t Size() const
    {
#ifdef EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        return m_back - m_front;
    }

    void Clear()
    {
        HTL_LOGT(ThreadId, "\n==================\n-> CLEAR and reset queue boundaries\n==================\n");
        m_back = 0;
        m_front = 0;
    }

    void PushBack(Job* job)
    {
#ifdef EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        int_fast32_t jobIndex = m_back;
        m_entries[jobIndex] = job;
        m_back.fetch_add(1);
        HTL_LOGT(ThreadId, "=> pushed_back job " << job->GetName() << " for size: " << (m_back - m_front) <<
            ", front: " << m_front <<
            ", back: " << m_back <<
            ", index: " << (jobIndex));
    }

    // pull from private FIFO end, allows to return not executable job for reordering
    Job* PopFront(bool allowOpenDependencies = false)
    {
#ifdef EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        int_fast32_t front = m_front;
        int_fast32_t back = m_back;
        if (front < back)
        {
            Job* job = m_entries[front];
            if (allowOpenDependencies || job->CanExecute())
            {
                // bool r = x.compare_exchange_*(&expected, T desired)
                // is the same as
                // bool r = atomic_compare_exchange_strong(atomic<T>*x, *expected, T desired)
                // compares x with expected (value)
                //  if equal -> x becomes desired and return true
                //  else -> expected is updated with actual value
                // weak does not quarantee successfull exchange
                if (!m_front.compare_exchange_strong(front, front + 1))
                    //if (!std::atomic_compare_exchange_strong(&m_front, &front, front + 1))
                {
                    // concurrent thread changed front -> should not happen
                    HTL_LOGT(ThreadId, "\n\nCOULD NOT POP FRONT");
                    return nullptr;
                }

                // TODO: check if this is still needed
                if (front == m_back && front != 0)
                {
                    HTL_LOGTW(ThreadId, "\n\nPOPPING FRONT last element, resetting boundaries: " << m_front << " - " << m_back);
                    m_back = 0;
                    m_front = 0;
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
        int_fast32_t back = m_back - 1;
        int_fast32_t front = m_front;
        if (front <= back)
        {
            Job* job = m_entries[back];
            // do not return a job if cannot execute
            if (!job->CanExecute())
            {
                return nullptr;
            }

            HTL_LOGT(ThreadId, "<= pop_back job size: " << (m_back - m_front) <<
                ", front: " << front <<
                ", back: " << back);

            if (!std::atomic_compare_exchange_strong(&m_back, &back, back))
            {
                // concurrent thread changed back -> main_looper called PushBack
                HTL_LOGT(ThreadId, "\n\nCOULD NOT POP BACK -> BACK changed");
                return nullptr;
            }

            if (front != back)
            {
                // still > 0 jobs left in the queue
                HTL_LOGT(ThreadId, "  found job: " << (job->GetName()) << "; remaining size: " << (m_back - m_front));
                return job;
            }
            else
            {
                // popping the last element in the queue
                if (!std::atomic_compare_exchange_strong(&m_front, &front, front))
                {
                    // concurrent thread changed front -> main_looper called PushBack
                    HTL_LOGT(ThreadId, "\n\nCOULD NOT POP BACK -> front changed");
                    return nullptr;
                }

                // TODO: check if this is still needed
                if (front == m_back && front != 0)
                {
                    HTL_LOGTW(ThreadId, "\n\nPOPPING BACK last element, resetting boundaries: " << m_front << " - " << m_back);
                    m_back = 0;
                    m_front = 0;
                }
                return job;
            }
        }
        else
        {
            //m_back = front;
            return nullptr; // queue already empty
        }
    }

    // Debug functionality for printing additional information
    // should get stripped awy by compiler if not used
    uint32_t ThreadId{ 0 };

    void Print() const
    {
#ifdef EXTRA_LOCKS
        lock_guard lock(mJobDequeMutex);
#endif
        HTL_LOGT(ThreadId, " -> current deque (" << (m_back - m_front) << "): ");
        for (size_t i = m_front; i < m_back; ++i)
        {
            HTL_LOGT(ThreadId, "\t" << m_entries[i]->GetName() << " - " << m_entries[i]->GetUnfinishedJobs());
        }
    }
};