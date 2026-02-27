load("@rules_cc//cc:defs.bzl", "cc_library")

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
    include_prefix = "flagd/evaluator/json_logic",
    strip_include_prefix = ".",
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status:statusor",
        "@nlohmann_json//:json",
    ],
)
