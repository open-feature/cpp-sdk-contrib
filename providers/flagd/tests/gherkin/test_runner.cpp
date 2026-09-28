#include <cstdlib>
#include <cucumber.hpp>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// cwt-cucumber's umbrella header does not re-export this one.
#include "test_results.hpp"  // for cuke::results::test_status

namespace {

// Rewrites "--flag=value" into separate "--flag" "value" arguments, which is
// the only form cwt-cucumber's option parser accepts. Returns true when the
// argument belonged to `flag`, so the caller knows not to forward it verbatim.
bool TrySplitInlineValue(std::string_view arg, std::string_view flag,
                         std::vector<std::string>* args, bool* seen) {
  const std::string prefix = std::string(flag) + "=";
  if (!arg.starts_with(prefix)) {
    return false;
  }
  args->emplace_back(flag);
  args->emplace_back(arg.substr(prefix.size()));
  *seen = true;
  return true;
}

// Appends "--flag <value>" from `env_var`, unless the caller already passed
// the flag on the command line.
void AppendFromEnv(const char* env_var, std::string_view flag, bool already_set,
                   std::vector<std::string>* args) {
  if (already_set) {
    return;
  }
  const char* value = std::getenv(env_var);
  if (value == nullptr || *value == '\0') {
    return;
  }
  args->emplace_back(flag);
  args->emplace_back(value);
}

}  // namespace

int main(int argc, char* argv[]) {
  std::vector<std::string> args;
  args.reserve(static_cast<size_t>(argc) + 4);
  args.emplace_back(argv[0]);

  bool has_tags = false;
  bool has_name = false;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "-t" || arg == "--tags") {
      has_tags = true;
      args.emplace_back(arg);
    } else if (arg == "-n" || arg == "--name") {
      has_name = true;
      args.emplace_back(arg);
    } else if (TrySplitInlineValue(arg, "--tags", &args, &has_tags)) {
      continue;
    } else if (TrySplitInlineValue(arg, "--name", &args, &has_name)) {
      continue;
    } else {
      args.emplace_back(arg);
    }
  }

  AppendFromEnv("GHERKIN_TAGS", "--tags", has_tags, &args);
  AppendFromEnv("GHERKIN_NAME", "--name", has_name, &args);

  std::vector<const char*> argv_c;
  argv_c.reserve(args.size());
  for (const std::string& arg : args) {
    argv_c.push_back(arg.c_str());
  }

  const cuke::results::test_status status =
      cuke::entry_point(static_cast<int>(argv_c.size()), argv_c.data());

  return status == cuke::results::test_status::passed ? 0 : 1;
}
