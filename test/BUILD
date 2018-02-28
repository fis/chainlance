package(default_visibility = ["//visibility:public"])

py_library(
    name = "source",
    srcs = ["source.py"],
    data = glob(["warriors/**/*.bfjoust"]) + ["warriors.idx"],
    deps = ["//tools:runfiles"],
)

py_library(
    name = "reference",
    srcs = ["reference.py"],
    data = ["reference.data"],
    deps = [
        ":source",
        "//tools:runfiles",
    ],
)

py_library(
    name = "geartalk",
    srcs = ["geartalk.py"],
    deps = [
        "//:geartalk_py_proto",
        "@com_google_protobuf//:protobuf_python",
    ],
)

py_binary(
    name = "generate_reference_data",
    srcs = ["generate_reference_data.py"],
    deps = [":source"],
    data = ["egojsout.js"],
)

#sh_binary(
#    name = "warriors",
#    srcs = ["warriors.sh"],
#    data = glob(["warriors-code/**/*.bfjoust"]) + ["warriors.idx"],
#)

#genrule(
#    name = "reference",
#    srcs = ["egojsout.js"],
#    outs = ["reference.out"],
#    tools = [":warriors"],
#    cmd = """
#        ./$(location :warriors) | while read left right; do \
#          nodejs $(location egojsout.js) "$$left" "$$right"; \
#        done > $(location reference.out)
#    """,
#)

