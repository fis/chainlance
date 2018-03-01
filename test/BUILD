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
)

py_binary(
    name = "generate_reference_data",
    srcs = ["generate_reference_data.py"],
    deps = [":source"],
    data = ["egojsout.js"],
)
