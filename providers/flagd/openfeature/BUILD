load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@com_google_protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "sync_proto",
    srcs = ["schemas/protobuf/flagd/sync/v1/sync.proto"],
    strip_import_prefix = "schemas/protobuf",
    deps = [
        "@com_google_protobuf//:struct_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "flagd_sync_cc_proto",
    visibility = ["//providers/flagd:__subpackages__"],
    deps = [":sync_proto"],
)

cc_grpc_library(
    name = "flagd_sync_cc_grpc",
    srcs = [":sync_proto"],
    grpc_only = True,
    visibility = ["//providers/flagd:__subpackages__"],
    deps = [":flagd_sync_cc_proto"],
)
