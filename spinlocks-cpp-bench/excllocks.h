#ifndef EXCL_LOCKS_HPP
#define EXCL_LOCKS_HPP

#include <cassert>
#include <vector>
#include <atomic>
#include <mutex>

#include "os.h"

class Mutex
{
public:
    ALWAYS_INLINE void Enter()
    {
        Mtx.lock();
    }

    ALWAYS_INLINE void Leave()
    {
        Mtx.unlock();
    }

private:
    std::mutex Mtx;
};

#if (OS == UNIX)

#include <pthread.h>

class SpinLockPThread
{
public:
    ALWAYS_INLINE SpinLockPThread()
    {
        pthread_spin_init(&Lock, 0);
    }

    ALWAYS_INLINE void Enter()
    {
        pthread_spin_lock(&Lock);
    }

    ALWAYS_INLINE void Leave()
    {
        pthread_spin_unlock(&Lock);
    }

private:
    pthread_spinlock_t Lock;
};

#elif (OS == WIN)

class LockCriticalSection
{
public:
    ALWAYS_INLINE LockCriticalSection()
    {
        InitializeCriticalSection(&Cs);
    }

    ALWAYS_INLINE void Enter()
    {
        EnterCriticalSection(&Cs);
    }

    ALWAYS_INLINE void Leave()
    {
        LeaveCriticalSection(&Cs);
    }

private:
    CRITICAL_SECTION Cs;
};

#endif

class ScTasSpinLock
{
public:
    ALWAYS_INLINE void Enter()
    {
        while (Locked.exchange(true));
    }

    ALWAYS_INLINE void Leave()
    {
        Locked.store(false);
    }

private:
    std::atomic_bool Locked = {false};
};

class TasSpinLock
{
public:
    ALWAYS_INLINE void Enter()
    {
        while (Locked.exchange(true, std::memory_order_acquire));
    }

    ALWAYS_INLINE void Leave()
    {
        Locked.store(false, std::memory_order_release);
    }

private:
    std::atomic_bool Locked = {false};
};

class TTasSpinLock
{
public:
    ALWAYS_INLINE void Enter()
    {
        do
        {
            while (Locked.load(std::memory_order_relaxed));
        }
        while (Locked.exchange(true, std::memory_order_acquire));
    }

    ALWAYS_INLINE void Leave()
    {
        Locked.store(false, std::memory_order_release);
    }

private:
    std::atomic_bool Locked = {false};
};

class RelaxTTasSpinLock
{
public:
    ALWAYS_INLINE void Enter()
    {
        do
        {
            while (Locked.load(std::memory_order_relaxed))
                CpuRelax();
        }
        while (Locked.exchange(true, std::memory_order_acquire));
    }

    ALWAYS_INLINE void Leave()
    {
        Locked.store(false, std::memory_order_release);
    }

private:
    std::atomic_bool Locked = {false};
};

class ExpBoRelaxTTasSpinLock
{
public:
    ALWAYS_INLINE void Enter()
    {
        size_t curMaxDelay = MIN_BACKOFF_ITERS;

        while (true)
        {
            WaitUntilLockIsFree();

            if (Locked.exchange(true, std::memory_order_acquire))
                BackoffExp(curMaxDelay);
            else
                break;
        }
    }

    ALWAYS_INLINE void Leave()
    {
        Locked.store(false, std::memory_order_release);
    }

private:
    ALWAYS_INLINE void WaitUntilLockIsFree() const
    {
        size_t numIters = 0;

        while (Locked.load(std::memory_order_relaxed))
        {
            if (numIters < MAX_WAIT_ITERS)
            {
                numIters++;
                CpuRelax();
            }
            else
                YieldSleep();
        }
    }

public:
    std::atomic_bool Locked = {false};

private:
    static const size_t MAX_WAIT_ITERS = 0x10000;
    static const size_t MIN_BACKOFF_ITERS = 32;
};

class TicketSpinLock
{
public:
    ALWAYS_INLINE void Enter()
    {
        const auto myTicketNo = NextTicketNo.fetch_add(1, std::memory_order_relaxed);

        while (ServingTicketNo.load(std::memory_order_acquire) != myTicketNo)
            CpuRelax();
    }

    ALWAYS_INLINE void Leave()
    {
        // We can get around a more expensive read-modify-write operation
        // (std::atomic_size_t::fetch_add()), because noone can modify
        // ServingTicketNo while we're in the critical section.
        const auto newNo = ServingTicketNo.load(std::memory_order_relaxed)+1;
        ServingTicketNo.store(newNo, std::memory_order_release);
    }

private:
    alignas(CACHELINE_SIZE) std::atomic_size_t ServingTicketNo = {0};
    alignas(CACHELINE_SIZE) std::atomic_size_t NextTicketNo = {0};
};

static_assert(sizeof(TicketSpinLock) == 2*CACHELINE_SIZE, "");

class PropBoTicketSpinLock
{
public:
    ALWAYS_INLINE void Enter()
    {
        constexpr size_t BACKOFF_BASE = 10;
        const auto myTicketNo = NextTicketNo.fetch_add(1, std::memory_order_relaxed);

        while (true)
        {
            const auto servingTicketNo = ServingTicketNo.load(std::memory_order_acquire);
            if (servingTicketNo == myTicketNo)
                break;

            const size_t waitIters = BACKOFF_BASE*(myTicketNo-servingTicketNo);

            for (size_t i=0; i<waitIters; i++)
                CpuRelax();
        }
    }

    ALWAYS_INLINE void Leave()
    {
        const auto newNo = ServingTicketNo.load(std::memory_order_relaxed)+1;
        ServingTicketNo.store(newNo, std::memory_order_release);
    }

private:
    alignas(CACHELINE_SIZE) std::atomic_size_t ServingTicketNo = {0};
    alignas(CACHELINE_SIZE) std::atomic_size_t NextTicketNo = {0};
};

static_assert(sizeof(PropBoTicketSpinLock) == 2*CACHELINE_SIZE, "");

class AndersonSpinLock
{
public:
    AndersonSpinLock(size_t maxThreads=std::thread::hardware_concurrency()) :
        LockedFlags(maxThreads)
    {
        for (auto &flag : LockedFlags)
            flag.first = true;

        LockedFlags[0].first = false;
    }

    ALWAYS_INLINE void Enter()
    {
        const size_t index = NextFreeIdx.fetch_add(1)%LockedFlags.size();
        auto &flag = LockedFlags[index].first;

        // Ensure overflow never happens
        if (index == 0)
            NextFreeIdx -= LockedFlags.size();

        while (flag)
            CpuRelax();
            
        flag = true;
    }

    ALWAYS_INLINE void Leave()
    {
        const size_t idx = NextServingIdx.fetch_add(1);
        LockedFlags[idx%LockedFlags.size()].first = false;
    }

private:
    using PaddedFlag = std::pair<std::atomic_bool, uint8_t[CACHELINE_SIZE-sizeof(std::atomic_bool)]>;
    static_assert(sizeof(PaddedFlag) == CACHELINE_SIZE, "");

    alignas(CACHELINE_SIZE) std::vector<PaddedFlag> LockedFlags;
    alignas(CACHELINE_SIZE) std::atomic_size_t      NextFreeIdx = {0};
    alignas(CACHELINE_SIZE) std::atomic_size_t      NextServingIdx = {1};
};

class GraunkeAndThakkarSpinLock
{
public:
    GraunkeAndThakkarSpinLock(size_t maxThreads=std::thread::hardware_concurrency()) :
        LockedFlags(maxThreads)
    {
        for (auto &flag : LockedFlags)
            flag.first = 1;

        assert(Tail.is_lock_free());
        Tail = reinterpret_cast<uintptr_t>(&LockedFlags[0].first);
        assert((Tail&1) == 0); // Make sure there's space to store the old flag value in the LSB
    }

    ALWAYS_INLINE void Enter()
    {
        // Create new tail by chaining my synchronization variable into the list
        const auto &newFlag = LockedFlags[GetThreadIndex()].first;
        const auto newTail = reinterpret_cast<uintptr_t>(&newFlag)|static_cast<uintptr_t>(newFlag);
        const auto ahead = Tail.exchange(newTail);

        // Extract flag and old value of previous thread in line, so that we can wait for its completion
        const auto *aheadFlag = reinterpret_cast<std::atomic_uint16_t *>(ahead&(~static_cast<uintptr_t>(1)));
        const auto aheadValue = static_cast<uint16_t>(ahead&1);

        // Wait for previous thread in line to flip my synchronization variable
        while (aheadFlag->load() == aheadValue)
            CpuRelax();
    }
    
    ALWAYS_INLINE void Leave()
    {
        // Flipping synchronization variable enables next thread in line to enter CS
        auto &flag = LockedFlags[GetThreadIndex()].first;
        flag = !flag;
    }
    
private:
    ALWAYS_INLINE size_t GetThreadIndex() const
    {
        static std::atomic_size_t threadCounter = {0};
        thread_local size_t threadIdx = threadCounter++;
        assert(threadIdx < LockedFlags.size());
        return threadIdx;
    }

private:
    using PaddedFlag = std::pair<std::atomic_uint16_t, uint8_t[CACHELINE_SIZE-sizeof(std::atomic_uint16_t)]>;
    static_assert(sizeof(PaddedFlag) == CACHELINE_SIZE, "");

    // In the LSB the old value of the flag is stored
    alignas(CACHELINE_SIZE) std::atomic<uintptr_t>  Tail;
    alignas(CACHELINE_SIZE) std::vector<PaddedFlag> LockedFlags;

    static_assert(sizeof(decltype(LockedFlags)::value_type) > 1,
                  "Flag size > 1 required: thanks to alginment, old flag value can be stored in LSB");
};

class McsLock
{
public:
    struct QNode
    {
        std::atomic<QNode *> Next = {nullptr};
        std::atomic_bool     Locked = {false};
    };

public:
    ALWAYS_INLINE void Enter(QNode &node)
    {
        node.Next = nullptr;
        node.Locked = true;

        QNode *oldTail = Tail.exchange(&node);

        if (oldTail != nullptr)
        {
            oldTail->Next = &node;

            while (node.Locked == true)
                CpuRelax();
        }
    }

    ALWAYS_INLINE void Leave(QNode &node)
    {
        if (node.Next.load() == nullptr)
        {
            QNode *tailWasMe = &node;
            if (Tail.compare_exchange_strong(tailWasMe, nullptr))
                return;
            
            while (node.Next.load() == nullptr)
                CpuRelax();
        }

        node.Next.load()->Locked = false;
    }

private:
    std::atomic<QNode *> Tail = {nullptr};
};

#endif
