load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "json_schema_validator",
    srcs = [
        "src/json-patch.cpp",
        "src/json-schema-draft7.json.cpp",
        "src/json-uri.cpp",
        "src/json-validator.cpp",
        "src/smtp-address-validator.cpp",
        "src/string-format-check.cpp",
    ],
    hdrs = [
        "src/json-patch.hpp",
        "src/nlohmann/json-schema.hpp",
        "src/smtp-address-validator.hpp",
    ],
    copts = ["-fexceptions"],
    features = ["-use_header_modules"],
    strip_include_prefix = "src",
    visibility = ["@//providers/flagd:__subpackages__"],
    deps = [
        "@nlohmann_json//:json",
    ],
)
