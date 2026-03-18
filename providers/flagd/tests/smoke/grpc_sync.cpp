#include "flagd/sync/grpc/grpc_sync.h"

#include <iostream>
#include <thread>

#include "flagd/configuration.h"

int main() {
  flagd::GrpcSync test = flagd::GrpcSync(flagd::FlagdProviderConfig());

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();

  test.Init(ctx).IgnoreError();

  for (int i = 0; i < 20; i++) {
    auto config = test.GetFlags();

    if (config->contains("myBoolFlag")) {
      std::cout << "Flag found!\n";
    } else {
      std::cout << "Flag not found (yet).\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
  }

  test.Shutdown().IgnoreError();
  return 0;
}
