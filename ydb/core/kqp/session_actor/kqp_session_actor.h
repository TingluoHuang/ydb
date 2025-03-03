#pragma once

#include <ydb/core/kqp/common/simple/temp_tables.h>
#include <ydb/core/kqp/counters/kqp_counters.h>
#include <ydb/core/kqp/federated_query/kqp_federated_query_helpers.h>
#include <ydb/core/kqp/gateway/kqp_gateway.h>
#include <ydb/core/protos/config.pb.h>
#include <ydb/library/yql/dq/actors/compute/dq_compute_actor_async_io_factory.h>

#include <ydb/library/actors/core/actorid.h>

namespace NKikimr::NKqp {

struct TKqpWorkerSettings {
    TString Cluster;
    TString Database;
    bool LongSession = false;

    NKikimrConfig::TTableServiceConfig TableService;
    NKikimrConfig::TQueryServiceConfig QueryService;

    TKqpDbCountersPtr DbCounters;

    TKqpWorkerSettings(const TString& cluster, const TString& database,
                       const NKikimrConfig::TTableServiceConfig& tableServiceConfig,
                       const  NKikimrConfig::TQueryServiceConfig& queryServiceConfig,
                       TKqpDbCountersPtr dbCounters)
        : Cluster(cluster)
        , Database(database)
        , TableService(tableServiceConfig)
        , QueryService(queryServiceConfig)
        , DbCounters(dbCounters) {}
};

IActor* CreateKqpSessionActor(const TActorId& owner, const TString& sessionId,
    const TKqpSettings::TConstPtr& kqpSettings, const TKqpWorkerSettings& workerSettings,
    std::optional<TKqpFederatedQuerySetup> federatedQuerySetup,
    NYql::NDq::IDqAsyncIoFactory::TPtr asyncIoFactory,
    TIntrusivePtr<TModuleResolverState> moduleResolverState, TIntrusivePtr<TKqpCounters> counters,
    const NKikimrConfig::TQueryServiceConfig& queryServiceConfig,
    const NKikimrConfig::TMetadataProviderConfig& metadataProviderConfig
    );

IActor* CreateKqpTempTablesManager(
    TKqpTempTablesState tempTablesState, const TActorId& target, const TString& database);

}  // namespace NKikimr::NKqp
