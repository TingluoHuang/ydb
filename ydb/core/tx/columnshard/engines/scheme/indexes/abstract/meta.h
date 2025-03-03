#pragma once
#include "checker.h"
#include "program.h"

#include <ydb/core/tx/columnshard/splitter/chunks.h>
#include <ydb/core/protos/flat_scheme_op.pb.h>
#include <ydb/services/bg_tasks/abstract/interface.h>

#include <library/cpp/object_factory/object_factory.h>

namespace NYql::NNodes {
class TExprBase;
}

namespace NKikimr::NOlap {
struct TIndexInfo;
class TProgramContainer;
}

namespace NKikimr::NSchemeShard {
class TOlapSchema;
}

namespace NKikimr::NOlap::NIndexes {

class IIndexMeta {
private:
    YDB_READONLY(ui32, IndexId, 0);
protected:
    virtual std::shared_ptr<IPortionDataChunk> DoBuildIndex(const ui32 indexId, std::map<ui32, std::vector<std::shared_ptr<IPortionDataChunk>>>& data, const TIndexInfo& indexInfo) const = 0;
    virtual void DoFillIndexCheckers(const std::shared_ptr<NRequest::TDataForIndexesCheckers>& info, const NSchemeShard::TOlapSchema& schema) const = 0;
    virtual bool DoDeserializeFromProto(const NKikimrSchemeOp::TOlapIndexDescription& proto) = 0;
    virtual void DoSerializeToProto(NKikimrSchemeOp::TOlapIndexDescription& proto) const = 0;

public:
    using TFactory = NObjectFactory::TObjectFactory<IIndexMeta, TString>;
    using TProto = NKikimrSchemeOp::TOlapIndexDescription;

    IIndexMeta() = default;
    IIndexMeta(const ui32 indexId)
        : IndexId(indexId)
    {

    }

    virtual ~IIndexMeta() = default;

    std::shared_ptr<IPortionDataChunk> BuildIndex(const ui32 indexId, std::map<ui32, std::vector<std::shared_ptr<IPortionDataChunk>>>& data, const TIndexInfo& indexInfo) const {
        return DoBuildIndex(indexId, data, indexInfo);
    }

    void FillIndexCheckers(const std::shared_ptr<NRequest::TDataForIndexesCheckers>& info, const NSchemeShard::TOlapSchema& schema) const {
        return DoFillIndexCheckers(info, schema);
    }

    bool DeserializeFromProto(const NKikimrSchemeOp::TOlapIndexDescription& proto) {
        IndexId = proto.GetId();
        AFL_VERIFY(IndexId);
        return DoDeserializeFromProto(proto);
    }

    void SerializeToProto(NKikimrSchemeOp::TOlapIndexDescription& proto) const {
        AFL_VERIFY(IndexId);
        proto.SetId(IndexId);
        return DoSerializeToProto(proto);
    }

    virtual TString GetClassName() const = 0;
};

class TIndexMetaContainer: public NBackgroundTasks::TInterfaceProtoContainer<IIndexMeta> {
private:
    using TBase = NBackgroundTasks::TInterfaceProtoContainer<IIndexMeta>;
public:
    TIndexMetaContainer() = default;
    TIndexMetaContainer(const std::shared_ptr<IIndexMeta>& object)
        : TBase(object)
    {
        AFL_VERIFY(Object);
    }
};

class TPortionIndexChunk: public IPortionDataChunk {
private:
    using TBase = IPortionDataChunk;
    const TString Data;
protected:
    virtual const TString& DoGetData() const override {
        return Data;
    }
    virtual TString DoDebugString() const override {
        return "";
    }
    virtual std::vector<std::shared_ptr<IPortionDataChunk>> DoInternalSplit(const TColumnSaver& /*saver*/, const std::shared_ptr<NColumnShard::TSplitterCounters>& /*counters*/, const std::vector<ui64>& /*splitSizes*/) const override {
        return {};
    }
    virtual bool DoIsSplittable() const override {
        return false;
    }
    virtual std::optional<ui32> DoGetRecordsCount() const override {
        return {};
    }
    virtual std::shared_ptr<arrow::Scalar> DoGetFirstScalar() const override {
        return nullptr;
    }
    virtual std::shared_ptr<arrow::Scalar> DoGetLastScalar() const override {
        return nullptr;
    }
    virtual void DoAddIntoPortion(const TBlobRange& bRange, TPortionInfo& portionInfo) const override;
public:
    TPortionIndexChunk(const ui32 entityId, const TString& data)
        : TBase(entityId, 0) 
        , Data(data)
    {
    }

};

class TIndexByColumns: public IIndexMeta {
private:
    using TBase = IIndexMeta;
    std::shared_ptr<NArrow::NSerialization::ISerializer> Serializer;
protected:
    std::set<ui32> ColumnIds;
    virtual std::shared_ptr<arrow::RecordBatch> DoBuildIndexImpl(TChunkedBatchReader& reader) const = 0;

    virtual std::shared_ptr<IPortionDataChunk> DoBuildIndex(const ui32 indexId, std::map<ui32, std::vector<std::shared_ptr<IPortionDataChunk>>>& data, const TIndexInfo& indexInfo) const override final;
    virtual bool DoDeserializeFromProto(const NKikimrSchemeOp::TOlapIndexDescription& /*proto*/) override;
public:
    TIndexByColumns() = default;
    TIndexByColumns(const ui32 indexId, const std::set<ui32>& columnIds);
};

}   // namespace NKikimr::NOlap::NIndexes