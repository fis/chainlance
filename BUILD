package(default_visibility = ["//visibility:public"])

# please build with the customized toolchain (see options in tools/bazel.rc)

# recommended binaries

SHARED_SRCS = ["common.c", "common.h", "parser.c", "parser.h"]
GEARLANCE_SRCS = SHARED_SRCS + ["gearlance.c", "gearlance.h"]

cc_binary(
    name = "gearlance",
    srcs = GEARLANCE_SRCS,
)

cc_binary(
    name = "cranklance",
    srcs = GEARLANCE_SRCS,
    copts = ["-DCRANK_IT=1"],
)

GEARLANCED_SRCS = GEARLANCE_SRCS + [
    "gearlanced.c",
    "geartalk.pb.c",
    "geartalk.pb.h",
]
GEARLANCED_OPTS = [
    "-DNO_MAIN=1",
    "-DPARSE_STDIN=1",
]

cc_binary(
    name = "gearlanced",
    srcs = GEARLANCED_SRCS,
    copts = GEARLANCED_OPTS,
    deps = ["@com_github_nanopb_nanopb//:nanopb"],
)

cc_binary(
    name = "cranklanced",
    srcs = GEARLANCED_SRCS,
    copts = GEARLANCED_OPTS + ["-DCRANK_IT=1"],
    deps = ["@com_github_nanopb_nanopb//:nanopb"],
)

# nanopb and Python (for tests) code generation for geartalk.proto

genrule(
    name = "geartalk_nanopb_codegen",
    srcs = ["geartalk.proto", "geartalk.options"],
    outs = ["geartalk.pb.c", "geartalk.pb.h"],
    tools = [
        "@com_github_nanopb_nanopb//:protoc-gen-nanopb",
        "@com_google_protobuf//:protoc",
    ],
    cmd = """
        ./$(location @com_google_protobuf//:protoc) \
          --plugin=protoc-gen-nanopb=$(location @com_github_nanopb_nanopb//:protoc-gen-nanopb) \
          --nanopb_out=$$(dirname $(location geartalk.pb.c)) \
          -Igeartalk.proto=$(location geartalk.proto) geartalk.proto; \
        sed -i -e 's|#include <pb.h>|#include \"pb.h\"|' $(location geartalk.pb.h)
    """,
)

# TODO(fis): use Bazel native py_proto_library once it exists
genrule(
    name = "geartalk_py_proto_codegen",
    srcs = ["geartalk.proto"],
    outs = ["geartalk_pb2.py"],
    tools = ["@com_google_protobuf//:protoc"],
    cmd = """
        ./$(location @com_google_protobuf//:protoc) \
          --python_out=$$(dirname $(location geartalk_pb2.py)) \
          -Igeartalk.proto=$(location geartalk.proto) geartalk.proto
    """,
)

py_library(
    name = "geartalk_py_proto",
    srcs = ["geartalk_pb2.py"],
    deps = ["@com_google_protobuf//:protobuf_python"],
)

# obsolete binaries

cc_binary(
    name = "chainlance",
    srcs = ["chainlance.c"],
)

cc_binary(
    name = "torquelance",
    srcs = [
        "common.c",
        "common.h",
        "parser.h",
        "torquelance.c",
        "torquelance-header.s",
    ],
)

cc_binary(
    name = "torquelance-compile",
    srcs = SHARED_SRCS + [
        "torquelance-compile.c",
        "torquelance-ops.s",
    ],
    copts = ["-DPARSER_EXTRAFIELDS='unsigned code;'"],
)

cc_binary(
    name = "wrenchlance-left",
    srcs = SHARED_SRCS + [
        "wrenchlance-left.c",
        "wrenchlance-ops.s",
    ],
    copts = ["-DPARSER_EXTRAFIELDS='unsigned code;'"],
)

cc_binary(
    name = "wrenchlance-right",
    srcs = SHARED_SRCS + ["wrenchlance-right.c"],
)

# tests

py_test(
    name = "gearlance_test",
    srcs = ["gearlance_test.py"],
    deps = ["//test:reference", "//test:source", "//tools:runfiles"],
    data = [":gearlance"],
)

py_test(
    name = "gearlanced_test",
    srcs = ["gearlanced_test.py"],
    deps = ["//test:geartalk", "//test:reference", "//test:source", "//tools:runfiles"],
    data = [":gearlanced"],
)

# TODO: debug
# - actual match fails in debug builds due to &size_header being a large number
# - compiler fails in --config=opt build, probably due same AV(...) thing
py_test(
    name = "torquelance_test",
    srcs = ["torquelance_test.py"],
    deps = ["//test:reference", "//test:source", "//tools:runfiles"],
    data = [":torquelance", ":torquelance-compile"],
)

# TODO: wrenchlance-left also fails in --config=opt build
py_test(
    name = "wrenchlance_test",
    srcs = ["wrenchlance_test.py"],
    deps = ["//test:reference", "//test:source", "//tools:runfiles"],
    data = [
        "common.c",
        "common.h",
        "parser.h",
        "wrenchlance-header.s",
        "wrenchlance-stub.c",
        ":wrenchlance-left",
        ":wrenchlance-right"
    ],
)
