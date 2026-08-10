#pragma once

#include <cstdint>
#include <string>

namespace evolab {

struct CliArgs {
  bool headless = false;
  bool showHelp = false;
  bool showVersion = false;
  bool exitAfterRun = false;

  std::uint64_t seed = 42;
  int resolution = 128;
  int frames = 120;
};

CliArgs parseCliArgs(int argc, char** argv);
void printHelp();
void printVersion();
int runHeadlessSmoke(const CliArgs& args);

}  // namespace evolab
