#pragma once

#include "sim/CellPopulation.hpp"
#include "sim/Energon.hpp"

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace evolab {

class BarrenWorld;

struct SessionDebugSummary {
  bool logOpened = false;
  std::uint64_t finalTick = 0;
  int maxPopulation = 0;
  int birthsDetected = 0;
  std::uint64_t firstBirthTick = 0;
  std::uint32_t firstChildId = 0;
  int oracleId = 0;
  bool ok = false;
};

// Interval-based session log for evo-lab --debug MS.
// Deletes any existing log on open; one file per run (evo-lab.session.log).
class SessionDebugLog {
public:
  static constexpr const char* kDefaultFileName = "evo-lab.session.log";

  bool open(const std::string& directory, int intervalMs, std::uint64_t seed,
            const char* archetypeLabel);
  void close();

  bool active() const { return out_.is_open(); }

  int intervalMs() const { return intervalMs_; }

  int birthsDetectedSoFar() const { return birthsDetected_; }

  // Log on sim-tick boundaries (intervalMs converted to ticks at 60 Hz).
  void onSimTick(std::uint64_t simTick, const BarrenWorld& world, const EnergonField& energon,
                 const CellPopulation& cells, float sunIntensity);

  SessionDebugSummary finalize();

private:
  void writeSnapshot(std::uint64_t simTick, const BarrenWorld& world, const EnergonField& energon,
                     const CellPopulation& cells, float sunIntensity, const char* eventLabel);
  void writeOrganismRecord(const Organism& organism, bool oracleDetail);

  std::ofstream out_;
  std::string path_;
  int intervalMs_ = 0;
  int intervalTicks_ = 1;
  std::uint64_t nextLogTick_ = 0;
  std::uint64_t seed_ = 0;
  int maxPopulation_ = 0;
  int birthsDetected_ = 0;
  std::uint64_t firstBirthTick_ = 0;
  std::uint32_t firstChildId_ = 0;
  int oracleId_ = 0;
  std::uint32_t maxSeedId_ = 0;
  std::unordered_map<std::uint32_t, std::uint32_t> offspringSeen_;
  bool headerWritten_ = false;
};

std::string sessionDebugLogPath(const std::string& directory);

SessionDebugSummary analyzeSessionDebugLog(const std::string& path);

}  // namespace evolab
