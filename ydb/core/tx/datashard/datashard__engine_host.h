#pragma once

#include "defs.h"
#include "change_collector.h"
#include "key_validator.h"

#include <ydb/core/kqp/runtime/kqp_tasks_runner.h>
#include <ydb/core/tablet_flat/tablet_flat_executor.h>
#include <ydb/core/engine/mkql_engine_flat.h>
#include <ydb/core/engine/minikql/minikql_engine_host.h>
#include <ydb/core/tx/datashard/datashard_kqp_compute.h>

namespace NKikimr {

namespace NTabletFlatExecutor {
    class TTransactionContext;
}

using NTabletFlatExecutor::TTransactionContext;

namespace NDataShard {

class TDataShard;

TIntrusivePtr<TThrRefBase> InitDataShardSysTables(TDataShard* self);

class TLockedWriteLimitException : public yexception {};

///
class TEngineBay : TNonCopyable {
public:
    using TValidationInfo = NMiniKQL::IEngineFlat::TValidationInfo;
    using TValidatedKey = NMiniKQL::IEngineFlat::TValidatedKey;
    using EResult = NMiniKQL::IEngineFlat::EResult;
    using TEngineHostCounters = NMiniKQL::TEngineHostCounters;

    struct TSizes {
        ui64 ReadSize = 0;
        ui64 ReplySize = 0;
        THashMap<ui64, ui64> OutReadSetSize;
        ui64 TotalKeysSize = 0;
    };

    TEngineBay(TDataShard * self, TTransactionContext& txc, const TActorContext& ctx,
               std::pair<ui64, ui64> stepTxId);

    virtual ~TEngineBay();

    const NMiniKQL::IEngineFlat * GetEngine() const { return Engine.Get(); }
    NMiniKQL::IEngineFlat * GetEngine();
    NMiniKQL::TEngineHost * GetEngineHost() { return EngineHost.Get(); }
    void SetLockTxId(ui64 lockTxId, ui32 lockNodeId);
    void SetUseLlvmRuntime(bool llvmRuntime) { EngineSettings->LlvmRuntime = llvmRuntime; }

    EResult Validate() {
        if (KeyValidator.GetInfo().Loaded)
            return EResult::Ok;
        Y_ABORT_UNLESS(Engine);
        return Engine->Validate(KeyValidator.GetInfo());
    }

    EResult ReValidateKeys() {
        Y_ABORT_UNLESS(KeyValidator.GetInfo().Loaded);
        Y_ABORT_UNLESS(Engine);
        return Engine->ValidateKeys(KeyValidator.GetInfo());
    }

    void MarkTxLoaded() {
        KeyValidator.GetInfo().SetLoaded();
    }

    /// @note it expects TValidationInfo keys are materialized outsize of engine's allocs
    void DestroyEngine() {
        ComputeCtx->Clear();
        if (KqpTasksRunner) {
            KqpTasksRunner.Reset();
            {
                auto guard = TGuard(*KqpAlloc);
                KqpTypeEnv.Reset();
            }
            KqpAlloc.Reset();
        }
        KqpExecCtx = {};

        Engine.Reset();
        EngineHost.Reset();
    }

    ui64 GetStep() const { return StepTxId.first; }
    ui64 GetTxId() const { return StepTxId.second; }

    TKeyValidator& GetKeyValidator() { return KeyValidator; }
    const TKeyValidator& GetKeyValidator() const { return KeyValidator; }
    const TValidationInfo& TxInfo() const { return KeyValidator.GetInfo(); }
    TEngineBay::TSizes CalcSizes(bool needsTotalKeysSize) const;

    void SetWriteVersion(TRowVersion writeVersion);
    void SetReadVersion(TRowVersion readVersion);
    void SetVolatileTxId(ui64 txId);
    void SetIsImmediateTx();
    void SetIsRepeatableSnapshot();

    void CommitChanges(const TTableId& tableId, ui64 lockId, const TRowVersion& writeVersion);

    TVector<IDataShardChangeCollector::TChange> GetCollectedChanges() const;
    void ResetCollectedChanges();

    TVector<ui64> GetVolatileCommitTxIds() const;
    const absl::flat_hash_set<ui64>& GetVolatileDependencies() const;
    std::optional<ui64> GetVolatileChangeGroup() const;
    bool GetVolatileCommitOrdered() const;

    void ResetCounters() { EngineHostCounters = TEngineHostCounters(); }
    const TEngineHostCounters& GetCounters() const { return EngineHostCounters; }

    NKqp::TKqpTasksRunner& GetKqpTasksRunner(NKikimrTxDataShard::TKqpTransaction& tx);
    NMiniKQL::TKqpDatashardComputeContext& GetKqpComputeCtx();

private:
    std::pair<ui64, ui64> StepTxId;
    THolder<NMiniKQL::TEngineHost> EngineHost;
    THolder<NMiniKQL::TEngineFlatSettings> EngineSettings;
    THolder<NMiniKQL::IEngineFlat> Engine;
    TKeyValidator KeyValidator;
    TEngineHostCounters EngineHostCounters;
    ui64 LockTxId;
    ui32 LockNodeId;
    NYql::NDq::TLogFunc KqpLogFunc;
    THolder<NUdf::IApplyContext> KqpApplyCtx;
    THolder<NMiniKQL::TKqpDatashardComputeContext> ComputeCtx;
    THolder<NMiniKQL::TScopedAlloc> KqpAlloc;
    THolder<NMiniKQL::TTypeEnvironment> KqpTypeEnv;
    NYql::NDq::TDqTaskRunnerContext KqpExecCtx;
    TIntrusivePtr<NKqp::TKqpTasksRunner> KqpTasksRunner;
};

}}
