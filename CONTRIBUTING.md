# Contributing to OpenFeature C++ Contrib

Thank you for your interest in contributing to the OpenFeature C++ SDK Contrib project. Here you can find info on the build system, review process, and other useful resources that might help you along the way.

## Development

### Target version(s)

We are using [version C++17](https://en.cppreference.com/w/cpp/17.html).

### Build system

We use **Bazel** as our primary build system. Ensure you have [Bazelisk](https://github.com/bazelbuild/bazelisk) installed to automatically manage the correct Bazel version (defined in `.bazelversion`).

### Build and Test

To build all targets in the repository:

```
bazel build //...
```

To run all tests:

```
bazel test //...
```

For individual providers (e.g., flagd):

```
bazel test //providers/flagd/...
```

## Coding Standards

### Code Style

We strictly follow the project's `.clang-format` configuration. Before submitting a PR, please format your code.

### Static Analysis

We use `clang-tidy` for static analysis. Our configuration is in `.clang-tidy`. While CI will enforce this, running it locally is encouraged. You can generate a `compile_commands.json` using the `hedron_compile_commands` target included in the workspace:

```bash
bazel run @hedron_compile_commands//:refresh_all
```

## Pull Request Process

1.  **Fork** the repository and create your branch from `main`.
```
git clone https://github.com/open-feature/cpp-sdk-contrib.git openfeature-cpp-sdk-contrib
cd openfeature-cpp-sdk-contrib
git remote add fork https://github.com/YOUR_GITHUB_USERNAME/cpp-sdk-contrib.git
git checkout -b feat/NAME_OF_FEATURE
```
2.  **Implement** your changes. Ensure you include unit tests for any new logic.
3.  **Format** your code using `clang-format`.
```
find . -name '*.h' -o -name '*.cpp' | xargs clang-format -i
```
4.  **Verify** that all tests pass locally.
```
bazel test //...
```
5.  **Commit** your changes. Use descriptive commit messages and remember to sign-off your commits with `commit -s` flag.
```
git commit -m "..." -s
```
6.  **Push** to your fork and submit a **Pull Request**. Remember that PR title must adhere to [Conventional Commits specification](https://www.conventionalcommits.org/en/v1.0.0/#specification).
```
git push fork feat/NAME_OF_FEATURE
```

## Collaboration Standards

Main points:
- Adhere to the [OpenFeature Specification](https://openfeature.dev/specification/).
- Minimize external dependencies. When adding a new dependency, prefer those already available in the Bazel workspace (see `MODULE.bazel`).
- Favor readable, maintainable C++ over "clever" implementations.

### How to Receive Comments

- If the PR is not ready for review, please mark it as
  [`draft`](https://github.blog/2019-02-14-introducing-draft-pull-requests/).
- Make sure all required CI checks are clear.
- Submit small, focused PRs addressing a single concern/issue.
- Make sure the PR title reflects the contribution.
- Write a summary that explains the change.
- Include usage examples in the summary, where applicable.

### How to Get PRs Merged

A PR is considered to be **ready to merge** when:

- Major feedback is resolved.
- Urgent fix can take exception as long as it has been actively communicated.

Any Maintainer can merge the PR once it is **ready to merge**. Note, that some
PRs may not be merged immediately if the repo is in the process of a release and
the maintainers decided to defer the PR to the next release train.

If a PR has been stuck (e.g. there are lots of debates and people couldn't agree
on each other), the owner should try to get people aligned by:

- Consolidating the perspectives and putting a summary in the PR. It is
  recommended to add a link into the PR description, which points to a comment
  with a summary in the PR conversation.
- Tagging domain experts (by looking at the change history) in the PR asking
  for suggestion.
- Reaching out to more people on the [CNCF OpenFeature Slack channel](https://cloud-native.slack.com/archives/C0344AANLA1).
- Stepping back to see if it makes sense to narrow down the scope of the PR or
  split it up.
- If none of the above worked and the PR has been stuck for more than 2 weeks,
  the owner should bring it to the OpenFeatures biweekly meeting.

## Design Choices

If you have questions about the design choices in specific interfaces or solutions:

* For questions specific to the C++ implementation, please refer to the documentation
  and issues in the [cpp-sdk repository](https://github.com/open-feature/cpp-sdk).
* For questions general to the core concepts and APIs, please refer to the
  [OpenFeature Specification](https://openfeature.dev/specification/).

**Note:** If your question isn't answered by these resources, please reach out to other
community members over our [slack channel](https://cloud-native.slack.com/archives/C09G371DQBT).
