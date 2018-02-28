licenses(["notice"])
exports_files(["LICENSE.txt"])

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "nanopb",
    hdrs = ["pb.h", "pb_common.h", "pb_decode.h", "pb_encode.h"],
    srcs = ["pb_common.c", "pb_decode.c", "pb_encode.c"],
    defines = ["PB_FIELD_16BIT=1"],
    includes = ["."],
)

# N.B. this rule needs to copy the nanopb_generator.py script out of the
# 'generator/' subdirectory, otherwise Python import will see the __init__.py in
# the source generator/proto/ directory and attempt to load the generated
# proto code from there, which won't work.

genrule(
    name = "generator_script",
    srcs = [
        "generator/nanopb_generator.py",
        "generator/proto/nanopb.proto",
        "generator/proto/plugin.proto",
        "generator/proto/google/protobuf/descriptor.proto",
    ],
    outs = [
        "nanopb_generator/protoc-gen-nanopb.py",
        "nanopb_generator/proto/__init__.py",
        "nanopb_generator/proto/nanopb_pb2.py",
        "nanopb_generator/proto/plugin_pb2.py",
    ],
    tools = ["@com_google_protobuf//:protoc"],
    cmd = """
        out="$$(dirname $(location nanopb_generator/protoc-gen-nanopb.py))"; \
        mkdir -p "$$out/proto"; \
        cp "$(location generator/nanopb_generator.py)" "$(location nanopb_generator/protoc-gen-nanopb.py)"; \
        touch "$(location nanopb_generator/proto/__init__.py)"; \
        ./$(location @com_google_protobuf//:protoc) --python_out="$$out/proto" \
          -Igoogle/protobuf/descriptor.proto="$(location generator/proto/google/protobuf/descriptor.proto)" \
          -Inanopb.proto="$(location generator/proto/nanopb.proto)" \
          -Iplugin.proto="$(location generator/proto/plugin.proto)" \
          nanopb.proto plugin.proto
    """,
    visibility = ["//visibility:private"],
)

py_binary(
    name = "protoc-gen-nanopb",
    srcs = [":generator_script"],
    deps = ["@com_google_protobuf//:protobuf_python"],
    main = "nanopb_generator/protoc-gen-nanopb.py",
    imports = ["nanopb_generator"],
)
