load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "sync_test",
    srcs = ["sync.cpp"],
    deps = [
        "//providers/flagd:flagd_configuration",
        "//providers/flagd:sync",
    ],
)
