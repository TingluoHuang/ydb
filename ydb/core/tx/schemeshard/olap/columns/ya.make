LIBRARY()

SRCS(
    schema.cpp
    update.cpp
)

PEERDIR(
    ydb/core/protos
    ydb/core/formats/arrow/dictionary
    ydb/core/formats/arrow/compression
)

YQL_LAST_ABI_VERSION()

END()
