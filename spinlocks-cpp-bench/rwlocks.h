#ifndef RW_LOCKS_HPP
#define RW_LOCKS_HPP

#include <cassert>
#include <vector>
#include <atomic>

#include "excllocks.h"

class SpinRwLockNaive
{
public:
    ALWAYS_INLINE void EnterExcl()
    {
        WriteLock.Enter();

        while (NumReaders > 0)
            CpuRelax();
    }

    ALWAYS_INLINE void LeaveExcl()
    {
        WriteLock.Leave();
    }

    ALWAYS_INLINE void EnterShared()
    {
        while (true)
        {
            NumReaders++;

            if (WriteLock.Locked)
                NumReaders--;
            else
                break;
        }
    }

    ALWAYS_INLINE void LeaveShared()
    {
        NumReaders--;
    }

private:
    ExpBoRelaxTTasSpinLock WriteLock;
    std::atomic_size_t       NumReaders = {0};
};

class SpinRwLockNaivePerThreadReadCounts
{
private:
    struct alignas(CACHELINE_SIZE) ReaderCounter
    {
        std::atomic_size_t Val;
    };

public:
    ALWAYS_INLINE SpinRwLockNaivePerThreadReadCounts() :
        ReaderCounters(std::thread::hardware_concurrency()*2)
    {
        // Verify that we have a power-of-2 number of reader counters
        assert((ReaderCounters.size()&(ReaderCounters.size()-1)) == 0);
    }

    ALWAYS_INLINE void EnterExcl()
    {
        WriteLock.Enter();

        for (const auto &rc : ReaderCounters)
            while (rc.Val > 0)
                CpuRelax();
    }

    ALWAYS_INLINE void LeaveExcl()
    {
        WriteLock.Leave();
    }

    ALWAYS_INLINE void EnterShared()
    {
        auto &rc = GetThreadReaderCount();

        while (true)
        {
            rc.Val++;

            if (WriteLock.Locked)
                rc.Val--;
            else
                break;
        }
    }

    ALWAYS_INLINE void LeaveShared()
    {
        auto &rc = GetThreadReaderCount();
        rc.Val--;
    }

private:
    size_t GetThreadIdx() const
    {
        const std::hash<std::thread::id> hashFn{};
        return hashFn(std::this_thread::get_id())&(ReaderCounters.size()-1);
    }

    ReaderCounter & GetThreadReaderCount()
    {
        return ReaderCounters[GetThreadIdx()];
    }

private:
    ExpBoRelaxTTasSpinLock    WriteLock;
    std::vector<ReaderCounter>  ReaderCounters;
};

class SpinRwLockNaivePerThreadReadCountsMemOrder
{
private:
    struct alignas(CACHELINE_SIZE) ReaderCounter
    {
        std::atomic_size_t Val;
    };

public:
    ALWAYS_INLINE SpinRwLockNaivePerThreadReadCountsMemOrder() :
        ReaderCounters(std::thread::hardware_concurrency()*2)
    {
        // Verify that we have a power-of-2 number of reader counters
        assert((ReaderCounters.size()&(ReaderCounters.size()-1)) == 0);
    }

    ALWAYS_INLINE void EnterExcl()
    {
        WriteLock.Enter();
        size_t waitIters = 1;

        for (const auto &rc : ReaderCounters)
            while (rc.Val.load(std::memory_order_relaxed) > 0)
                BackoffExp(waitIters);

        std::atomic_thread_fence(std::memory_order_acquire);
    }

    ALWAYS_INLINE void LeaveExcl()
    {
        WriteLock.Leave();
    }

    ALWAYS_INLINE void EnterShared()
    {
        auto &rc = GetThreadReaderCount();

        while (true)
        {
            rc.Val.fetch_add(1, std::memory_order_relaxed);

            if (WriteLock.Locked.load(std::memory_order_relaxed))
                rc.Val.fetch_sub(1, std::memory_order_relaxed);
            else
                break;
        }

        std::atomic_thread_fence(std::memory_order_acquire);
    }

    ALWAYS_INLINE void LeaveShared()
    {
        auto &rc = GetThreadReaderCount();
        rc.Val.fetch_sub(1, std::memory_order_release);
    }

private:
    size_t GetThreadIdx() const
    {
        const std::hash<std::thread::id> hashFn{};
        return hashFn(std::this_thread::get_id())&(ReaderCounters.size()-1);
    }

    ReaderCounter & GetThreadReaderCount()
    {
        return ReaderCounters[GetThreadIdx()];
    }

private:
    ExpBoRelaxTTasSpinLock      WriteLock;
    std::vector<ReaderCounter>  ReaderCounters;
};

#endif