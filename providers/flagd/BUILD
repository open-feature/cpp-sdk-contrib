cc_library(
    name = "flagd_provider",

    # TODO: Remove recursive glob and replace with individual Bazel packages
    # Source files from the src/ directory
    srcs = glob(["src/**/*.cpp"]),

    # Header files from the include/ directory
    hdrs = glob(["include/**/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "@openfeature_cpp_sdk",
    ],
)
