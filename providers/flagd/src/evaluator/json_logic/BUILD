load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "json_logic",
    srcs = [
        "array.cpp",
        "data.cpp",
        "json_logic.cpp",
        "logic.cpp",
        "numeric.cpp",
        "string_ops.cpp",
        "utils.cpp",
    ],
    hdrs = [
        "array.h",
        "data.h",
        "json_logic.h",
        "logic.h",
        "numeric.h",
        "string_ops.h",
        "utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@nlohmann_json//:json",
    ],
)

cc_binary(
    name = "cli",
    srcs = ["cli.cpp"],
    deps = [
        ":json_logic",
        "@abseil-cpp//absl/status",
        "@nlohmann_json//:json",
    ],
)
