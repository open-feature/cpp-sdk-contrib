#include <cstdlib>
#include <cucumber.hpp>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// cwt-cucumber internal headers are included directly to satisfy clang-tidy
// misc-include-cleaner, as the library's umbrella header <cucumber.hpp>
// does not explicitly export them.
#include "test_results.hpp"  // for cuke::results::test_status

int main(int argc, char* argv[]) {
  std::vector<std::string> args;
  args.reserve(argc + 4);
  args.push_back(argv[0]);

  bool has_tags = false;
  bool has_name = false;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "-t" || arg == "--tags") {
      has_tags = true;
      args.push_back(argv[i]);
    } else if (arg.rfind("--tags=", 0) == 0) {
      has_tags = true;
      args.push_back("--tags");
      args.push_back(std::string(arg.substr(7)));
    } else if (arg == "-n" || arg == "--name") {
      has_name = true;
      args.push_back(argv[i]);
    } else if (arg.rfind("--name=", 0) == 0) {
      has_name = true;
      args.push_back("--name");
      args.push_back(std::string(arg.substr(7)));
    } else {
      args.push_back(argv[i]);
    }
  }

  if (!has_tags) {
    if (const char* env_tags = std::getenv("GHERKIN_TAGS")) {
      if (*env_tags != '\0') {
        args.push_back("--tags");
        args.push_back(env_tags);
      }
    }
  }

  if (!has_name) {
    if (const char* env_name = std::getenv("GHERKIN_NAME")) {
      if (*env_name != '\0') {
        args.push_back("--name");
        args.push_back(env_name);
      }
    }
  }

  std::cout << "Running Gherkin tests with " << args.size() - 1
            << " arguments.\n";
  for (size_t i = 1; i < args.size(); ++i) {
    std::cout << "  arg[" << i << "]: " << args[i] << '\n';
  }

  std::vector<const char*> argv_c;
  argv_c.reserve(args.size());
  for (const auto& a : args) {
    argv_c.push_back(a.c_str());
  }

  cuke::results::test_status status =
      cuke::entry_point(static_cast<int>(argv_c.size()), argv_c.data());

  return status == cuke::results::test_status::passed ? 0 : 1;
}
