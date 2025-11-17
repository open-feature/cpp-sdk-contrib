# Contributing

## Development

### Target version(s)

We are using [version C++17](https://en.cppreference.com/w/cpp/17.html).

### Installation and Dependencies

The code will be build, tested and managed using [Bazel build system](https://bazel.build/).

## Pull Request

All contributions to the OpenFeature project are welcome via GitHub pull requests.

To create a new PR, you will need to first fork the GitHub repository and clone upstream.

```bash
git clone https://github.com/open-feature/cpp-sdk-contrib.git openfeature-cpp-sdk-contrib
```

Navigate to the repository folder

```bash
cd openfeature-cpp-sdk-contrib
```

Add your fork as an origin

```bash
git remote add fork https://github.com/YOUR_GITHUB_USERNAME/cpp-sdk-contrib.git
```

Ensure your development environment is all set up by building and testing

```bash
make
```

To start working on a new feature or bugfix, create a new branch and start working on it.

```bash
git checkout -b feat/NAME_OF_FEATURE
# Make your changes
git commit
git push fork feat/NAME_OF_FEATURE
```

Open a pull request against the main python-sdk-contrib repository.

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
