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

std::uint64_t parseSeed(std::string_view value) {
  try {
    return std::stoull(std::string(value));
  } catch (...) {
    throw std::runtime_error("Invalid value for --seed: " + std::string(value));
  }
}

}  // namespace

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
    } else {
      throw std::runtime_error("Unknown argument: " + std::string(arg));
    }
  }
  return args;
}

void printHelp() {
  std::cout << "evo-lab — Phase 0 barren world simulator\n\n"
            << "Usage:\n"
            << "  evo-lab [options]\n\n"
            << "Options:\n"
            << "  --headless           Run without opening a window\n"
            << "  --frames N           Simulation ticks to run (default: 120)\n"
            << "  --seed N             World seed (default: 42)\n"
            << "  --resolution N       Heightmap resolution (default: 128)\n"
            << "  --exit               Exit after headless run (for smoke tests)\n"
            << "  --version            Print version and exit\n"
            << "  --help               Show this help\n\n"
            << "Interactive controls:\n"
            << "  drag left mouse      orbit camera\n"
            << "  scroll               zoom\n"
            << "  Space                pause/resume tide\n"
            << "  R                    regenerate world\n";
}

void printVersion() { std::cout << "evo-lab 0.1.0 (Phase 0 — barren world)\n"; }

}  // namespace evolab
