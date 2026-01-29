# JSON Logic

This is a C++ implementation of [JSON Logic](https://jsonlogic.com/).

## Why a custom implementation?

We decided to create our own implementation because existing C++ libraries for JSON Logic often lack a straightforward way to supply custom operators. Specifically, we need support for flagd custom features:
- [Fractional Operation](https://flagd.dev/reference/custom-operations/fractional-operation/)
- [Semantic Version Operation](https://flagd.dev/reference/custom-operations/semver-operation/)
- [Starts-With / Ends-With Operation](https://flagd.dev/reference/custom-operations/string-comparison-operation/)

## Future Plans

This implementation might be published as a separate repository in the future, solely dedicated to `json_logic`.

## Specification

This implementation follows the assumptions and specifications defined in the official [JSON Logic spec](https://jsonlogic.com/).
