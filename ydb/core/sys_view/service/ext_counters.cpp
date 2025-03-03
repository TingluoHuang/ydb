#include "ext_counters.h"

#include <ydb/core/base/appdata.h>
#include <ydb/core/base/counters.h>
#include <ydb/core/graph/api/events.h>
#include <ydb/core/graph/api/service.h>
#include <ydb/library/services/services.pb.h>
#include <ydb/library/actors/core/actor_bootstrapped.h>


namespace NKikimr {
namespace NSysView {

class TExtCountersUpdaterActor
    : public TActorBootstrapped<TExtCountersUpdaterActor>
{
    using TCounterPtr = ::NMonitoring::TDynamicCounters::TCounterPtr;
    using THistogramPtr = ::NMonitoring::THistogramPtr;
    using THistogramSnapshotPtr = ::NMonitoring::IHistogramSnapshotPtr;

    const TExtCountersConfig Config;

    TCounterPtr MemoryUsedBytes;
    TCounterPtr MemoryLimitBytes;
    TCounterPtr StorageUsedBytes;
    TVector<TCounterPtr> CpuUsedCorePercents;
    TVector<TCounterPtr> CpuLimitCorePercents;
    THistogramPtr ExecuteLatencyMs;

    TCounterPtr AnonRssSize;
    TCounterPtr CGroupMemLimit;
    TVector<TCounterPtr> PoolElapsedMicrosec;
    TVector<TCounterPtr> PoolCurrentThreadCount;
    TVector<ui64> PoolElapsedMicrosecPrevValue;
    TVector<ui64> ExecuteLatencyMsValues;
    TVector<ui64> ExecuteLatencyMsPrevValues;
    TVector<ui64> ExecuteLatencyMsBounds;

public:
    static constexpr NKikimrServices::TActivity::EType ActorActivityType() {
        return NKikimrServices::TActivity::EXT_COUNTERS_UPDATER_ACTOR;
    }

    explicit TExtCountersUpdaterActor(TExtCountersConfig&& config)
        : Config(std::move(config))
    {}

    void Bootstrap() {
        auto ydbGroup = GetServiceCounters(AppData()->Counters, "ydb");

        MemoryUsedBytes = ydbGroup->GetNamedCounter("name",
            "resources.memory.used_bytes", false);
        MemoryLimitBytes = ydbGroup->GetNamedCounter("name",
            "resources.memory.limit_bytes", false);
        StorageUsedBytes = ydbGroup->GetNamedCounter("name",
            "resources.storage.used_bytes", false);

        auto poolCount = Config.Pools.size();
        CpuUsedCorePercents.resize(poolCount);
        CpuLimitCorePercents.resize(poolCount);

        for (size_t i = 0; i < poolCount; ++i) {
            auto name = to_lower(Config.Pools[i].Name);
            CpuUsedCorePercents[i] = ydbGroup->GetSubgroup("pool", name)->GetNamedCounter("name",
                "resources.cpu.used_core_percents", true);
            CpuLimitCorePercents[i] = ydbGroup->GetSubgroup("pool", name)->GetNamedCounter("name",
                "resources.cpu.limit_core_percents", false);
        }

        ExecuteLatencyMs = ydbGroup->FindNamedHistogram("name", "table.query.execution.latency_milliseconds");

        Schedule(TDuration::Seconds(1), new TEvents::TEvWakeup);
        Become(&TThis::StateWork);
    }

private:
    void Initialize() {
        if (!AnonRssSize) {
            auto utilsGroup = GetServiceCounters(AppData()->Counters, "utils");

            AnonRssSize = utilsGroup->FindCounter("Process/AnonRssSize");
            CGroupMemLimit = utilsGroup->FindCounter("Process/CGroupMemLimit");

            PoolElapsedMicrosec.resize(Config.Pools.size());
            PoolCurrentThreadCount.resize(Config.Pools.size());
            PoolElapsedMicrosecPrevValue.resize(Config.Pools.size());
            for (size_t i = 0; i < Config.Pools.size(); ++i) {
                auto poolGroup = utilsGroup->FindSubgroup("execpool", Config.Pools[i].Name);
                if (poolGroup) {
                    PoolElapsedMicrosec[i] = poolGroup->FindCounter("ElapsedMicrosec");
                    PoolCurrentThreadCount[i] = poolGroup->FindCounter("CurrentThreadCount");
                    if (PoolElapsedMicrosec[i]) {
                        PoolElapsedMicrosecPrevValue[i] = PoolElapsedMicrosec[i]->Val();
                    }
                }
            }
        }
    }

    void Transform() {
        Initialize();
        auto metrics(MakeHolder<NGraph::TEvGraph::TEvSendMetrics>());
        if (AnonRssSize) {
            MemoryUsedBytes->Set(AnonRssSize->Val());
            metrics->AddMetric("resources.memory.used_bytes", AnonRssSize->Val());
        }
        if (CGroupMemLimit) {
            MemoryLimitBytes->Set(CGroupMemLimit->Val());
        }
        if (StorageUsedBytes->Val() != 0) {
            metrics->AddMetric("resources.storage.used_bytes", StorageUsedBytes->Val());
        }
        double cpuUsage = 0;
        for (size_t i = 0; i < Config.Pools.size(); ++i) {
            if (PoolElapsedMicrosec[i]) {
                auto elapsedMs = PoolElapsedMicrosec[i]->Val();
                double usedCore = elapsedMs / 10000.;
                CpuUsedCorePercents[i]->Set(usedCore);
                if (PoolElapsedMicrosecPrevValue[i] != 0) {
                    cpuUsage += (elapsedMs - PoolElapsedMicrosecPrevValue[i]) / 1000000.;
                }
                PoolElapsedMicrosecPrevValue[i] = elapsedMs;
            }
            if (PoolCurrentThreadCount[i] && PoolCurrentThreadCount[i]->Val()) {
                double limitCore = PoolCurrentThreadCount[i]->Val() * 100;
                CpuLimitCorePercents[i]->Set(limitCore);
            } else {
                double limitCore = Config.Pools[i].ThreadCount * 100;
                CpuLimitCorePercents[i]->Set(limitCore);
            }
        }
        metrics->AddMetric("resources.cpu.usage", cpuUsage);
        if (ExecuteLatencyMs) {
            THistogramSnapshotPtr snapshot = ExecuteLatencyMs->Snapshot();
            ui32 count = snapshot->Count();
            if (ExecuteLatencyMsValues.empty()) {
                ExecuteLatencyMsValues.resize(count);
                ExecuteLatencyMsPrevValues.resize(count);
                ExecuteLatencyMsBounds.resize(count);
            }
            ui64 total = 0;
            for (ui32 n = 0; n < count; ++n) {
                ui64 value = snapshot->Value(n);;
                ui64 diff = value - ExecuteLatencyMsPrevValues[n];
                total += diff;
                ExecuteLatencyMsValues[n] = diff;
                ExecuteLatencyMsPrevValues[n] = value;
                if (ExecuteLatencyMsBounds[n] == 0) {
                    ExecuteLatencyMsBounds[n] = snapshot->UpperBound(n);
                }
            }
            metrics->AddMetric("queries.requests", total);
            if (total != 0) {
                double p50 = NGraph::GetTimingForPercentile(50, ExecuteLatencyMsValues, ExecuteLatencyMsBounds, total);
                if (!isnan(p50)) {
                    metrics->AddMetric("queries.latencies.p50", p50);
                }
                double p75 = NGraph::GetTimingForPercentile(75, ExecuteLatencyMsValues, ExecuteLatencyMsBounds, total);
                if (!isnan(p75)) {
                    metrics->AddMetric("queries.latencies.p75", p75);
                }
                double p90 = NGraph::GetTimingForPercentile(90, ExecuteLatencyMsValues, ExecuteLatencyMsBounds, total);
                if (!isnan(p90)) {
                    metrics->AddMetric("queries.latencies.p90", p90);
                }
                double p99 = NGraph::GetTimingForPercentile(99, ExecuteLatencyMsValues, ExecuteLatencyMsBounds, total);
                if (!isnan(p99)) {
                    metrics->AddMetric("queries.latencies.p99", p99);
                }
            }
        }
        Send(NGraph::MakeGraphServiceId(), metrics.Release());
    }

    void HandleWakeup() {
        Schedule(TDuration::Seconds(1), new TEvents::TEvWakeup);
        Transform();
    }

    STRICT_STFUNC(StateWork, {
        cFunc(TEvents::TSystem::Wakeup, HandleWakeup);
    })
};

IActor* CreateExtCountersUpdater(TExtCountersConfig&& config) {
    return new TExtCountersUpdaterActor(std::move(config));
}

}
}

