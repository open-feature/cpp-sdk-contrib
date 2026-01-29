load("@rules_python//python:defs.bzl", "py_binary")
load("@rules_cc//cc:defs.bzl", "cc_library")

py_binary(
    name = "generate_schema_header_bin",
    srcs = ["generate_schema_header.py"],
    main = "generate_schema_header.py",
    visibility = ["//visibility:private"],
)

genrule(
    name = "generate_schema_header",
    srcs = ["@flagd-schemas//:json_schemas"],
    outs = ["embedded_schemas.h"],
    cmd = "$(location :generate_schema_header_bin) --output $@ $(SRCS)",
    tools = [":generate_schema_header_bin"],
)

cc_library(
    name = "embedded_schemas",
    hdrs = ["embedded_schemas.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
)
