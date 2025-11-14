package(default_visibility = ["//visibility:public"])

cc_library(
    name = "flagd_provider",
    # Source files from the src/ directory
    srcs = glob(["src/**/*.cpp"]),

    # Header files from the include/ directory
    hdrs = glob(["include/**/*.h"]),
    includes = ["include"],
    deps = [
        "@openfeature_cpp_sdk",
    ],
)
