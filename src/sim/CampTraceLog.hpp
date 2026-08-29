#pragma once

#include "sim/Organism.hpp"
#include "sim/PerceptorFocus.hpp"

#include <cstdint>
#include <fstream>
#include <string>

namespace evolab {

// Per-tick trace for a single CAMP camper (chemotaxis / foraging harness).
class CampTraceLog {
public:
  static constexpr const char* kDefaultFileName = "camp-chemotaxis.trace";

  bool open(const std::string& path);
  void close();

  bool active() const { return out_.is_open(); }

  void writeHeader(std::uint64_t seed, float targetX, float targetZ);

  void recordTick(std::uint64_t simTick, const Organism& organism, float targetX, float targetZ,
                  float sunIntensity);

private:
  static std::uint8_t inboundAxonByte(const Organism& organism, std::uint32_t srcId,
                                      std::uint32_t dstId, std::uint64_t simTick);

  std::ofstream out_;
};

}  // namespace evolab
