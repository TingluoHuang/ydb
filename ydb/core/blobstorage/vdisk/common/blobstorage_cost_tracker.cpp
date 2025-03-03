#include "blobstorage_cost_tracker.h"

namespace NKikimr {

const TDiskOperationCostEstimator TBsCostModelBase::HDDEstimator{
    { 80000, 1.774 },   // ReadCoefficients
    { 6500, 11.1 },     // WriteCoefficients
    { 6.089e+06, 8.1 }, // HugeWriteCoefficients
};

class TBsCostModelMirror3dc : public TBsCostModelBase {
public:
    TBsCostModelMirror3dc(NPDisk::EDeviceType deviceType)
        : TBsCostModelBase(deviceType)
    {}
};

class TBsCostModel4Plus2Block : public TBsCostModelBase {
public:
    TBsCostModel4Plus2Block(NPDisk::EDeviceType deviceType)
        : TBsCostModelBase(deviceType)
    {}
};

class TBsCostModelMirror3of4 : public TBsCostModelBase {
public:
    TBsCostModelMirror3of4(NPDisk::EDeviceType deviceType)
        : TBsCostModelBase(deviceType)
    {}
};

TBsCostTracker::TBsCostTracker(const TBlobStorageGroupType& groupType, NPDisk::EDeviceType diskType,
        const TIntrusivePtr<::NMonitoring::TDynamicCounters>& counters)
    : GroupType(groupType)
    , CostCounters(counters->GetSubgroup("subsystem", "advancedCost"))
    , UserDiskCost(CostCounters->GetCounter("UserDiskCost", true))
    , CompactionDiskCost(CostCounters->GetCounter("CompactionDiskCost", true))
    , ScrubDiskCost(CostCounters->GetCounter("ScrubDiskCost", true))
    , DefragDiskCost(CostCounters->GetCounter("DefragDiskCost", true))
    , InternalDiskCost(CostCounters->GetCounter("InternalDiskCost", true))
{
    switch (GroupType.GetErasure()) {
    case TBlobStorageGroupType::ErasureMirror3dc:
        CostModel = std::make_unique<TBsCostModelMirror3dc>(diskType);
        break;
    case TBlobStorageGroupType::Erasure4Plus2Block:
        CostModel = std::make_unique<TBsCostModel4Plus2Block>(diskType);
        break;
    case TBlobStorageGroupType::ErasureMirror3of4:
        CostModel = std::make_unique<TBsCostModelMirror3of4>(diskType);
        break;
    default:
        CostModel = std::make_unique<TBsCostModelErasureNone>(diskType);
        break;
    }
}

} // NKikimr
