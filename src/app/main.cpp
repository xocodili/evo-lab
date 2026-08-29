#include "app/CliArgs.hpp"
#include "app/SmokeTest.hpp"
#include "app/VisualApp.hpp"

#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
  try {
    const evolab::CliArgs args = evolab::parseCliArgs(argc, argv);

    if (args.showHelp) {
      evolab::printHelp();
      return 0;
    }
    if (args.showVersion) {
      evolab::printVersion();
      return 0;
    }

    if (args.headless) {
      if (args.debugIntervalMs > 0) {
        return evolab::runHeadlessDebugSession(args);
      }
      return evolab::runHeadlessSmoke(args);
    }

    return evolab::runVisualApp(args);
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << '\n';
    return 2;
  }
}
