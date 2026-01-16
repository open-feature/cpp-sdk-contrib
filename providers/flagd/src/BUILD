load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "flagd_provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    include_prefix = "flagd",
    visibility = ["//visibility:public"],
    deps = [
        ":flagd_configuration",
        "@abseil-cpp//absl/status",
        "@openfeature_cpp_sdk//openfeature",
    ],
)

cc_library(
    name = "flagd_configuration",
    srcs = ["configuration.cpp"],
    hdrs = ["configuration.h"],
    include_prefix = "flagd",
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/strings",
        "@com_github_grpc_grpc//:grpc++",
        "@openfeature_cpp_sdk//openfeature",
    ],
)

cc_library(
    name = "sync",
    srcs = ["sync.cpp"],
    hdrs = ["sync.h"],
    include_prefix = "flagd",
    visibility = ["//visibility:public"],
    deps = [
        ":embedded_schemas",
        ":flagd_configuration",
        "@abseil-cpp//absl/status",
        "@com_github_grpc_grpc//:grpc++",
        "@flagd-schemas//:flagd_sync_cc_grpc",
        "@flagd-schemas//:flagd_sync_cc_proto",
        "@nlohmann_json//:json",
        "@nlohmann_json_schema_validator//:json_schema_validator",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
    ],
)

genrule(
    name = "generate_schema_header",
    srcs = ["@flagd-schemas//:json_schemas"],
    outs = ["embedded_schemas.h"],
    cmd = """
        echo '#ifndef FLAGD_JSON_EMBEDDED_SCHEMAS_H_' > $@
        echo '#define FLAGD_JSON_EMBEDDED_SCHEMAS_H_' >> $@
        echo '#include <map>' >> $@
        echo '#include <string>' >> $@
        echo 'namespace schema {' >> $@
        echo 'static const std::map<std::string, std::string> schemas = {' >> $@

        for f in $(SRCS); do
            name=$$(basename $$f)
            echo "{ \\\"$$name\\\", R\\\"raw(" >> $@
            cat $$f >> $@
            echo ")raw\\\" }," >> $@
        done

        echo '};' >> $@
        echo '}  // namespace schema' >> $@
        echo '#endif  // FLAGD_JSON_EMBEDDED_SCHEMAS_H_' >> $@
    """,
)

cc_library(
    name = "embedded_schemas",
    hdrs = ["embedded_schemas.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
)
