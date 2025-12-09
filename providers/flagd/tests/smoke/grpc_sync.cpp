#include <iostream>
#include <thread>

#include "flagd/configuration.h"
#include "flagd/sync.h"

int main() {
  flagd::GrpcSync test = flagd::GrpcSync(flagd::FlagdProviderConfig());

  test.Init(openfeature::EvaluationContext()).IgnoreError();

  for (int i = 0; i < 20; i++) {
    auto config = test.GetFlags();

    if (config->contains("myBoolFlag")) {
      std::cout << "Flag found!" << std::endl;
    } else {
      std::cout << "Flag not found (yet)." << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
  }

  test.Shutdown().IgnoreError();
  return 0;
}
