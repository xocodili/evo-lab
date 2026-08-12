#pragma once

#include "sim/SimConfig.hpp"

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

  SeedArchetype archetype = SeedArchetype::Nom;
  int nomCount = 60;
  float tidePeriodTicks = 0.0f;
  int designWidth = 1280;
  int designHeight = 720;
  float fixedSimHz = 60.0f;
};

SimConfig simConfigFromCli(const CliArgs& args);

CliArgs parseCliArgs(int argc, char** argv);
void printHelp();
void printVersion();
int runHeadlessSmoke(const CliArgs& args);

}  // namespace evolab
