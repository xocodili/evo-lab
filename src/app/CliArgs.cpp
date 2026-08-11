#include "app/CliArgs.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace evolab {

namespace {

int parsePositiveInt(std::string_view value, const char* name) {
  try {
    const int n = std::stoi(std::string(value));
    if (n <= 0) {
      throw std::invalid_argument("non-positive");
    }
    return n;
  } catch (...) {
    throw std::runtime_error(std::string("Invalid value for ") + name + ": " + std::string(value));
  }
}

float parsePositiveFloat(std::string_view value, const char* name) {
  try {
    const float n = std::stof(std::string(value));
    if (n <= 0.0f) {
      throw std::invalid_argument("non-positive");
    }
    return n;
  } catch (...) {
    throw std::runtime_error(std::string("Invalid value for ") + name + ": " + std::string(value));
  }
}

std::uint64_t parseSeed(std::string_view value) {
  try {
    return std::stoull(std::string(value));
  } catch (...) {
    throw std::runtime_error("Invalid value for --seed: " + std::string(value));
  }
}

}  // namespace

SimConfig simConfigFromCli(const CliArgs& args) {
  SimConfig config;
  config.seed = args.seed;
  config.resolution = args.resolution;
  config.archetype = args.archetype;
  config.nomCount = args.nomCount;
  config.tidePeriodTicks = args.tidePeriodTicks;
  config.designWidth = args.designWidth;
  config.designHeight = args.designHeight;
  config.fixedSimHz = args.fixedSimHz;
  return config;
}

CliArgs parseCliArgs(int argc, char** argv) {
  CliArgs args;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      args.showHelp = true;
    } else if (arg == "--version") {
      args.showVersion = true;
    } else if (arg == "--headless") {
      args.headless = true;
    } else if (arg == "--exit") {
      args.exitAfterRun = true;
    } else if (arg == "--frames") {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for --frames");
      }
      args.frames = parsePositiveInt(argv[++i], "--frames");
    } else if (arg == "--seed") {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for --seed");
      }
      args.seed = parseSeed(argv[++i]);
    } else if (arg == "--resolution") {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for --resolution");
      }
      args.resolution = parsePositiveInt(argv[++i], "--resolution");
    } else if (arg == "--archetype") {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for --archetype");
      }
      args.archetype = parseSeedArchetype(argv[++i]);
    } else if (arg == "--nom-count") {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for --nom-count");
      }
      args.nomCount = parsePositiveInt(argv[++i], "--nom-count");
    } else if (arg == "--tide-period") {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for --tide-period");
      }
      args.tidePeriodTicks = parsePositiveFloat(argv[++i], "--tide-period");
    } else if (arg == "--design-width") {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for --design-width");
      }
      args.designWidth = parsePositiveInt(argv[++i], "--design-width");
    } else if (arg == "--design-height") {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for --design-height");
      }
      args.designHeight = parsePositiveInt(argv[++i], "--design-height");
    } else if (arg == "--sim-hz") {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for --sim-hz");
      }
      args.fixedSimHz = parsePositiveFloat(argv[++i], "--sim-hz");
    } else {
      throw std::runtime_error("Unknown argument: " + std::string(arg));
    }
  }
  return args;
}

void printHelp() {
  std::cout << "evo-lab — Phase 2.x tidal world simulator\n\n"
            << "Usage:\n"
            << "  evo-lab [options]\n\n"
            << "Options:\n"
            << "  --headless           Run without opening a window\n"
            << "  --frames N           Simulation ticks to run (default: 120)\n"
            << "  --seed N             World seed (default: 42)\n"
            << "  --resolution N       Heightmap resolution (default: 128)\n"
            << "  --archetype NAME     stem | twomouth | actuator | ma | pma (default: pma)\n"
            << "  --nom-count N        Organisms to seed (default: 60)\n"
            << "  --tide-period N      Tide period in ticks (default: engine default)\n"
            << "  --design-width N     Design resolution width (default: 1280)\n"
            << "  --design-height N    Design resolution height (default: 720)\n"
            << "  --sim-hz N           Fixed simulation rate (default: 60)\n"
            << "  --exit               Exit after headless run (for smoke tests)\n"
            << "  --version            Print version and exit\n"
            << "  --help               Show this help\n\n"
            << "Interactive controls:\n"
            << "  drag left mouse      orbit camera\n"
            << "  WASD                 pan camera\n"
            << "  scroll               zoom\n"
            << "  Space                pause/resume sim\n"
            << "  R                    regenerate world\n";
}

void printVersion() { std::cout << "evo-lab 0.2.0 (Phase 2.x — Noms)\n"; }

}  // namespace evolab
