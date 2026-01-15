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
        echo '#include <string>' >> $@
        echo 'namespace schema {' >> $@

        for f in $(SRCS); do
            # 1. Get filename without extension (flagd.json -> flagd)
            name=$$(basename $$f .json | sed 's/-/_/g')
            
            # 2. Capitalize first letter (flagd -> Flagd)
            first_char=$${name:0:1}
            rest_chars=$${name:1}
            cap_name="$$(echo $$first_char | tr '[:lower:]' '[:upper:]')$$rest_chars"
            
            var_name="$${cap_name}Schema"

            # 3. Write C++ variable safely
            echo "constexpr const char* $$var_name = R\\\"raw(" >> $@
            cat $$f >> $@
            echo ")raw\\\";" >> $@
        done

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
