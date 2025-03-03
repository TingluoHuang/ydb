#include "utils.h"

#include <deque>
#include <util/string/builder.h>

#include <ydb/library/yverify_stream/yverify_stream.h>

namespace NKikimr::NPQ {

ui64 TopicPartitionReserveSize(const NKikimrPQ::TPQTabletConfig& config) {
    if (!config.HasMeteringMode()) {
        // Only for federative and dedicated installations
        return 0;
    }
    if (NKikimrPQ::TPQTabletConfig::METERING_MODE_REQUEST_UNITS == config.GetMeteringMode()) {
        return 0;
    }
    if (config.GetPartitionConfig().HasStorageLimitBytes()) {
        return config.GetPartitionConfig().GetStorageLimitBytes();
    }
    return config.GetPartitionConfig().GetLifetimeSeconds() * config.GetPartitionConfig().GetWriteSpeedInBytesPerSecond();
}

ui64 TopicPartitionReserveThroughput(const NKikimrPQ::TPQTabletConfig& config) {
    if (!config.HasMeteringMode()) {
        // Only for federative and dedicated installations
        return 0;
    }
    if (NKikimrPQ::TPQTabletConfig::METERING_MODE_REQUEST_UNITS == config.GetMeteringMode()) {
        return 0;
    }
    return config.GetPartitionConfig().GetWriteSpeedInBytesPerSecond();
}

bool SplitMergeEnabled(const NKikimrPQ::TPQTabletConfig& config) {
    return config.GetPartitionStrategy().GetMinPartitionCount() < config.GetPartitionStrategy().GetMaxPartitionCount(); // TODO
}

static constexpr ui64 PUT_UNIT_SIZE = 40960u; // 40Kb

ui64 PutUnitsSize(const ui64 size) {
    ui64 putUnitsCount = size / PUT_UNIT_SIZE;
    if (size % PUT_UNIT_SIZE != 0)
        ++putUnitsCount;    
    return putUnitsCount;        
}

const NKikimrPQ::TPQTabletConfig::TPartition* GetPartitionConfig(const NKikimrPQ::TPQTabletConfig& config, const ui32 partitionId) {
    for(const auto& p : config.GetPartitions()) {
        if (partitionId == p.GetPartitionId()) {
            return &p;
        }
    }
    return nullptr;
}

TPartitionGraph::TPartitionGraph() {
}

TPartitionGraph::TPartitionGraph(std::unordered_map<ui32, Node>&& partitions) {
    Partitions = std::move(partitions);
}

const TPartitionGraph::Node* TPartitionGraph::GetPartition(ui32 id) const {
    auto it = Partitions.find(id);
    if (it == Partitions.end()) {
        return nullptr;
    }
    return &it->second;
}

std::set<ui32> TPartitionGraph::GetActiveChildren(ui32 id) const {
    const auto* p = GetPartition(id);
    if (!p) {
        return {};
    }

    std::deque<const Node*> queue;
    queue.push_back(p);

    std::set<ui32> result;
    while(!queue.empty()) {
        const auto* n = queue.front();
        queue.pop_front();

        if (n->Children.empty()) {
            result.emplace(n->Id);
        } else {
            queue.insert(queue.end(), n->Children.begin(), n->Children.end());
        }
    }

    return result;
}

template<typename TPartition>
std::unordered_map<ui32, TPartitionGraph::Node> BuildGraph(const ::google::protobuf::RepeatedPtrField<TPartition>& partitions) {
    std::unordered_map<ui32, TPartitionGraph::Node> result;

    if (0 == partitions.size()) {
        return result;
    }

    for (const auto& p : partitions) {
        result.emplace(p.GetPartitionId(), TPartitionGraph::Node(p.GetPartitionId(), p.GetTabletId()));
    }

    std::deque<TPartitionGraph::Node*> queue;
    for(const auto& p : partitions) {
        auto& node = result[p.GetPartitionId()];

        node.Children.reserve(p.ChildPartitionIdsSize());
        for (auto id : p.GetChildPartitionIds()) {
            node.Children.push_back(&result[id]);
        }

        node.Parents.reserve(p.ParentPartitionIdsSize());
        for (auto id : p.GetParentPartitionIds()) {
            node.Parents.push_back(&result[id]);
        }

        if (p.GetParentPartitionIds().empty()) {
            queue.push_back(&node);
        }
    }

    while(!queue.empty()) {
        auto* n = queue.front();
        queue.pop_front();

        bool allCompleted = true;
        for(auto* c : n->Parents) {
            if (c->HierarhicalParents.empty() && !c->Parents.empty()) {
                allCompleted = false;
                break;
            }
        }

        if (allCompleted) {
            for(auto* c : n->Parents) {
                n->HierarhicalParents.insert(c->HierarhicalParents.begin(), c->HierarhicalParents.end());
                n->HierarhicalParents.insert(c);
            }
            queue.insert(queue.end(), n->Children.begin(), n->Children.end());
        }
    }

    return result;
}


TPartitionGraph::Node::Node(ui32 id, ui64 tabletId)
    : Id(id)
    , TabletId(tabletId) {
}

TPartitionGraph MakePartitionGraph(const NKikimrPQ::TPQTabletConfig& config) {
    return TPartitionGraph(BuildGraph<NKikimrPQ::TPQTabletConfig::TPartition>(config.GetAllPartitions()));
}

TPartitionGraph MakePartitionGraph(const NKikimrSchemeOp::TPersQueueGroupDescription& config) {
    return TPartitionGraph(BuildGraph<NKikimrSchemeOp::TPersQueueGroupDescription::TPartition>(config.GetPartitions()));
}

} // NKikimr::NPQ
