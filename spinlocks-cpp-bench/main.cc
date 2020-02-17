#include <functional>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <atomic>
#include <string>
#include <random>
#include <mutex>
#include <cmath>
#include <sys/uio.h>

#include "excllocks.h"
#include "rwlocks.h"
#include "./header.h"

#if 1
template<typename LockType>
std::vector<std::chrono::milliseconds> CreateBenchmarkRuns(size_t numRuns, size_t numItersPerRun, size_t numThreads, data_input* dataInput)
{
    std::vector<std::chrono::milliseconds> runs(numRuns);
    const size_t numItersPerThread = numItersPerRun/numThreads;
    volatile std::atomic_size_t cnt = {0};
    // build_iovecs(numThreads, dataInput);
    // std::cout << "\nlocaliovlen:" << dataInput->local[0].iov_len << ", remoteiovlen:" << dataInput->remote[0].iov_len << ".\n";
    // std::cout << "\nlocaliovlen:" << dataInput->local[1].iov_len << ", remoteiovlen:" << dataInput->remote[1].iov_len << ".\n";


    for (size_t i=0; i<numRuns; i++)
    {
        std::vector<std::future<void>> futures(numThreads);
        LockType lock;
        std::atomic_size_t numThreadsReady = {0};
        const auto startTime = std::chrono::high_resolution_clock::now();

        for (size_t j=0; j<numThreads; j++)
        {
            futures[j] = std::async(std::launch::async, [&, j]()
            {
                BindThisThreadToCore(j%5 + 1);

                // Wait until all threads are ready
                numThreadsReady++;
                while (numThreadsReady < numThreads)
                    CpuRelax();

                for (size_t k=0; k<numItersPerThread; k++)
                {
                    //McsLock::QNode node;
                    //lock.Enter(node);
                    lock.Enter();

                    // for (size_t h = 0; h < IOV_NUM; h++)
                    // {
                        long long nread = process_vm_writev(dataInput->pid, (&dataInput->local[j]), \
                        1, (&dataInput->remote[j]), 1, 0);
                        if (nread < 0)
                        {
                            /* code */
                            // std::cout << "thread_num:" << j << ":" << h << " ,write_num:"  << nread << "localiovlen:" << dataInput->local[h].iov_len << ", remoteiovlen:" << dataInput->remote[h].iov_len << ".\n";
                            perror("process_vm:");
                        }
                        // std::cout << "thread_num:" << j << " write_num:"  << nread << "localiovlen:" << dataInput->local[j].iov_len << ", remoteiovlen:" << dataInput->remote[j].iov_len << ".\n";
                    // }

                    // for (size_t l=0; l<16; l++)
                    //     cnt++;
                    //lock.Leave(node);
                    lock.Leave();
                }
            });
        }

        for (auto &f : futures)
            f.wait();

        const auto endTime = std::chrono::high_resolution_clock::now();
        runs[i] = std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime);
    }

    return runs;
}
#else
template<typename LockType>
std::vector<std::chrono::milliseconds> CreateBenchmarkRuns(size_t numRuns, size_t numItersPerRun, size_t numThreads)
{
    std::vector<std::chrono::milliseconds> runs(numRuns);
    const size_t numItersPerThread = numItersPerRun/numThreads;

    for (size_t i=0; i<numRuns; i++)
    {
        std::vector<std::future<void>> futures(numThreads);
        LockType lock;
        size_t count = 0;

        const auto startTime = std::chrono::high_resolution_clock::now();

        for (size_t j=0; j<numThreads; j++)
        {
            futures[j] = std::async(std::launch::async, [&]()
            {
                size_t localCount = 0;

                for (size_t k=0; k<numItersPerThread; k++)
                {
                    if ((k%16) == 0)
                    {
                        lock.EnterExcl();
                        count++;
                        for (volatile int bla=0; bla<10; bla++);
                        lock.LeaveExcl();
                    }
                    else
                    {
                        lock.EnterShared();
                        localCount += count;
                        for (volatile int bla=0; bla<10; bla++);
                        lock.LeaveShared();
                    }
                }
            });
        }

        for (auto &f : futures)
            f.wait();

        const auto endTime = std::chrono::high_resolution_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime);
        runs[i] = elapsedMs;
    }

    return runs;
}
#endif

template<typename LockType>
void RunBenchmark(const char *descr, size_t numRuns, size_t numItersPerRun, size_t numThreads, data_input* dataInput)
{
    const auto &runs = CreateBenchmarkRuns<LockType>(numRuns, numItersPerRun, numThreads, dataInput);
    double avgElapsedMs(0), varianceMs(0), minMs(std::numeric_limits<double>::max()), maxMs(0);

    for (const auto &r : runs)
    {
        const double curRunElapsedMs = static_cast<double>(r.count());
        minMs = std::min(minMs, curRunElapsedMs);
        maxMs = std::max(maxMs, curRunElapsedMs);
        avgElapsedMs += curRunElapsedMs;
    }

    avgElapsedMs /= static_cast<double>(runs.size());

    for (const auto &r : runs)
    {
        const double diff = static_cast<double>(r.count())-avgElapsedMs;
        varianceMs += diff*diff;
    }

    varianceMs /= static_cast<double>(runs.size());

    const double stdDevMs = std::sqrt(varianceMs);
    const double avgElapsedNs = avgElapsedMs*1000.0*1000.0;
    const double timePerIterNs = avgElapsedNs/(numRuns*numItersPerRun);

    std::cout << std::left << std::setfill(' ') << std::setw(30) << descr << "  "
              << std::fixed << std::setprecision(2) << std::right << std::setfill(' ') << std::setw(6)
              << avgElapsedMs << "   " << std::right << std::setw(6) << stdDevMs << "   "
              << std::right << std::setw(6) << minMs << "   " << std::right << std::setw(6) << maxMs 
              << "   " << std::right << std::setw(6) << timePerIterNs << "\n";
}

void RunBenchmarks(data_input* dataInput)
{
    std::cout << "                                           Std.                      Time/\n";
    std::cout << "                                 Avg.      dev.    Min      Max      iter.\n";
    std::cout << "Lock type                        (ms)      (ms)    (ms)     (ms)     (ns)\n";
    std::cout << "----------------------------------------------------------------------------\n\n";

    const auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i=1; i<=std::thread::hardware_concurrency(); i++)
    {
        // const size_t numRuns = 1;
        const size_t numRuns = 5;
        const size_t numItersPerRun = 10;
        // const size_t numItersPerRun = 15;
        build_iovecs(5, dataInput);

        std::cout << i << " Threads (work/thread: " << numItersPerRun/i << ")\n\n";

        RunBenchmark<Mutex>("Mutex", numRuns, numItersPerRun, i, dataInput);
#if (OS == UNIX)
        RunBenchmark<SpinLockPThread>("SpinLockPThread", numRuns, numItersPerRun, i, dataInput);
#elif (OS == WIN)
        RunBenchmark<LockCriticalSection>("LockCriticalSection", numRuns, numItersPerRun, i);
#endif
        RunBenchmark<ScTasSpinLock>("ScTasSpinLock", numRuns, numItersPerRun, i, dataInput);
        RunBenchmark<TasSpinLock>("TasSpinLock", numRuns, numItersPerRun, i, dataInput);
        RunBenchmark<TTasSpinLock>("TTasSpinLock", numRuns, numItersPerRun, i, dataInput);
        RunBenchmark<RelaxTTasSpinLock>("RelaxTTasSpinLock", numRuns, numItersPerRun, i, dataInput);
        RunBenchmark<ExpBoRelaxTTasSpinLock>("ExpBoRelaxTTasSpinLock", numRuns, numItersPerRun, i, dataInput);

        RunBenchmark<TicketSpinLock>("TicketSpinLock", numRuns, numItersPerRun, i, dataInput);
        RunBenchmark<PropBoTicketSpinLock>("PropBoTicketSpinLock", numRuns, numItersPerRun, i, dataInput);
        
        // RunBenchmark<AndersonSpinLock>("AndersonSpinLock", numRuns, numItersPerRun, i, dataInput);
        // RunBenchmark<GraunkeAndThakkarSpinLock>("GraunkeAndThakkarSpinLock", numRuns, numItersPerRun, i);
#if 0
        RunBenchmark<SpinRwLockNaive>("SpinRwLockNaive", numRuns, numItersPerRun, i);
        RunBenchmark<SpinRwLockNaivePerThreadReadCounts>("SpinRwLockNaivePerThreadReadCounts", numRuns, numItersPerRun, i);
#endif
        std::cout << "\n";
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "Total elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime).count() << " ms\n";
}

int main(int argc, char **argv)
{
    data_input dataInput;
    get_inputs(&dataInput, argc, argv);
    RunBenchmarks(&dataInput);
    return 0;
}
