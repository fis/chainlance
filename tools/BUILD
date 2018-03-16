package(default_visibility = ["//visibility:public"])

py_library(
    name = "runfiles",
    srcs = ["runfiles.py"],
)

# custom toolchain for pure C binaries

cc_toolchain_suite(
    name = "opt",
    toolchains = {
        "native|gcc": ":native_gcc_toolchain"
    },
)

filegroup(name = "empty")

cc_toolchain(
    name = "native_gcc_toolchain",
    all_files = ":empty",
    compiler_files = ":empty",
    cpu = "native",
    dwp_files = ":empty",
    dynamic_runtime_libs = [":empty"],
    linker_files = ":empty",
    objcopy_files = ":empty",
    static_runtime_libs = [":empty"],
    strip_files = ":empty",
    supports_param_files = 0,
)
