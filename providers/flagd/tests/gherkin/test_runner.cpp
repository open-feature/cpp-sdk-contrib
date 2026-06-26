#include <cucumber.hpp>
#include <iostream>
#include <vector>

// cwt-cucumber internal headers are included directly to satisfy clang-tidy
// misc-include-cleaner, as the library's umbrella header <cucumber.hpp>
// does not explicitly export them.
#include "test_results.hpp"  // for cuke::results::test_status

int main(int argc, char* argv[]) {
  std::cout << "Running Gherkin tests with " << argc - 1 << " arguments.\n";
  for (int i = 1; i < argc; ++i) {
    std::cout << "  arg[" << i << "]: " << argv[i] << '\n';
  }

  std::vector<const char*> argv_c(argc);
  for (int i = 0; i < argc; ++i) {
    argv_c[i] = argv[i];
  }

  cuke::results::test_status status = cuke::entry_point(argc, argv_c.data());

  return status == cuke::results::test_status::passed ? 0 : 1;
}
