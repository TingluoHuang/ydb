#include "executor_pool_shared.h"

#include "actorsystem.h"
#include "executor_pool_basic.h"
#include "executor_thread.h"
#include "executor_thread_ctx.h"

#include <atomic>
#include <ydb/library/actors/util/affinity.h>


namespace NActors {

TSharedExecutorPool::TSharedExecutorPool(const TSharedExecutorPoolConfig &config, i16 poolCount, std::vector<i16> poolsWithThreads)
    : ThreadByPool(poolCount, -1)
    , PoolByThread(poolsWithThreads.size())
    , BorrowedThreadByPool(poolCount, -1)
    , PoolByBorrowedThread(poolsWithThreads.size(), -1)
    , Pools(poolCount)
    , PoolCount(poolCount)
    , SharedThreadCount(poolsWithThreads.size())
    , Threads(new TSharedExecutorThreadCtx[SharedThreadCount])
    , Timers(new TTimers[SharedThreadCount])
    , TimePerMailbox(config.TimePerMailbox)
    , EventsPerMailbox(config.EventsPerMailbox)
    , SoftProcessingDurationTs(config.SoftProcessingDurationTs)
{
    for (ui32 poolIdx = 0, threadIdx = 0; poolIdx < poolsWithThreads.size(); ++poolIdx, ++threadIdx) {
        Y_ABORT_UNLESS(poolsWithThreads[poolIdx] < poolCount);
        ThreadByPool[poolsWithThreads[poolIdx]] = threadIdx;
        PoolByThread[threadIdx] = poolsWithThreads[poolIdx];
    }
}

void TSharedExecutorPool::Prepare(TActorSystem* actorSystem, NSchedulerQueue::TReader** scheduleReaders, ui32* scheduleSz) {
        // ActorSystem = actorSystem;

        ScheduleReaders.reset(new NSchedulerQueue::TReader[SharedThreadCount]);
        ScheduleWriters.reset(new NSchedulerQueue::TWriter[SharedThreadCount]);

        std::vector<IExecutorPool*> poolsBasic = actorSystem->GetBasicExecutorPools();
        std::vector<IExecutorPool*> poolByThread(SharedThreadCount);
        for (IExecutorPool* pool : poolsBasic) {
            Pools[pool->PoolId] = dynamic_cast<TBasicExecutorPool*>(pool);
            i16 threadIdx = ThreadByPool[pool->PoolId];
            if (threadIdx >= 0) {
                poolByThread[threadIdx] = pool;
            }
        }

        for (i16 i = 0; i != SharedThreadCount; ++i) {
            // !TODO
            Threads[i].ExecutorPools[0].store(dynamic_cast<TBasicExecutorPool*>(poolByThread[i]), std::memory_order_release);
            Threads[i].Thread.Reset(
                new TSharedExecutorThread(
                    -1,
                    actorSystem,
                    &Threads[i],
                    PoolCount,
                    "SharedThread",
                    SoftProcessingDurationTs,
                    TimePerMailbox,
                    EventsPerMailbox));
            ScheduleWriters[i].Init(ScheduleReaders[i]);
        }

        *scheduleReaders = ScheduleReaders.get();
        *scheduleSz = SharedThreadCount;
}

void TSharedExecutorPool::Start() {
    //ThreadUtilization = 0;
    //AtomicAdd(MaxUtilizationCounter, -(i64)GetCycleCountFast());

    for (i16 i = 0; i != SharedThreadCount; ++i) {
        Threads[i].Thread->Start();
    }
}

void TSharedExecutorPool::PrepareStop() {
    for (i16 i = 0; i != SharedThreadCount; ++i) {
        Threads[i].Thread->StopFlag = true;
        Threads[i].Interrupt();
    }
}

void TSharedExecutorPool::Shutdown() {
    for (i16 i = 0; i != SharedThreadCount; ++i) {
        Threads[i].Thread->Join();
    }
}

bool TSharedExecutorPool::Cleanup() {
    return true;
}

TSharedExecutorThreadCtx* TSharedExecutorPool::GetSharedThread(i16 pool) {
    i16 threadIdx = ThreadByPool[pool];
    if (threadIdx < 0 || threadIdx >= PoolCount) {
        return nullptr;
    }
    return &Threads[threadIdx];
}

void TSharedExecutorPool::ReturnHalfThread(i16 pool) {
    i16 threadIdx = ThreadByPool[pool];
    TBasicExecutorPool* borrowingPool = Threads[threadIdx].ExecutorPools[1].exchange(nullptr, std::memory_order_acq_rel);
    Y_ABORT_UNLESS(borrowingPool);
    BorrowedThreadByPool[PoolByBorrowedThread[threadIdx]] = -1;
    PoolByBorrowedThread[threadIdx] = -1;
    // TODO(kruall): Check on race
    borrowingPool->ReleaseSharedThread();
}

void TSharedExecutorPool::GiveHalfThread(i16 from, i16 to) {
    i16 threadIdx = ThreadByPool[from];
    TBasicExecutorPool* borrowingPool = Pools[to];
    Threads[threadIdx].ExecutorPools[1].store(borrowingPool, std::memory_order_release);
    BorrowedThreadByPool[to] = threadIdx;
    PoolByBorrowedThread[threadIdx] = to;
    // TODO(kruall): Check on race
    borrowingPool->AddSharedThread(&Threads[threadIdx]);
}

void TSharedExecutorPool::GetSharedStats(i16 poolId, std::vector<TExecutorThreadStats>& statsCopy) {
    statsCopy.resize(SharedThreadCount + 1);
    for (i16 i = 0; i < SharedThreadCount; ++i) {
        Threads[i].Thread->GetSharedStats(poolId, statsCopy[i + 1]);
    }
}

TCpuConsumption TSharedExecutorPool::GetThreadCpuConsumption(i16 poolId, i16 threadIdx) {
    if (threadIdx >= SharedThreadCount) {
        return {0.0, 0.0};
    }
    TExecutorThreadStats stats;
    Threads[threadIdx].Thread->GetSharedStats(poolId, stats);
    return {Ts2Us(stats.SafeElapsedTicks), static_cast<double>(stats.CpuUs), stats.NotEnoughCpuExecutions};
}

std::vector<TCpuConsumption> TSharedExecutorPool::GetThreadsCpuConsumption(i16 poolId) {
    std::vector<TCpuConsumption> result;
    for (i16 threadIdx = 0; threadIdx < SharedThreadCount; ++threadIdx) {
        result.push_back(GetThreadCpuConsumption(poolId, threadIdx));
    }
    return result;
}

i16 TSharedExecutorPool::GetSharedThreadCount() const {
    return SharedThreadCount;
}

}