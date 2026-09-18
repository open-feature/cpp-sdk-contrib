load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "qcbor",
    srcs = glob([
        "src/*.c",
        "src/*.h",
    ]),
    hdrs = glob([
        "inc/qcbor/*.h",
        "inc/*.h",
    ]),
    includes = ["inc"],
    copts = ["-include stdint.h"],
    visibility = ["//visibility:public"],
)
