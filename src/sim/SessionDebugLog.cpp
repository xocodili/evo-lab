#include "sim/SessionDebugLog.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/EnergonStats.hpp"
#include "sim/PerceptorFocus.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace evolab {

namespace {

std::uint32_t nodeStoreBytes(const Organism& organism, NeuronType type) {
  for (const SkeletonNode& node : organism.nodes) {
    if (node.neuron == type && node.alive) {
      return static_cast<std::uint32_t>(node.store.size());
    }
  }
  return 0;
}

const char* focusKindLabel(PerceptFocusKind kind) {
  switch (kind) {
    case PerceptFocusKind::Food:
      return "food";
    case PerceptFocusKind::Mate:
      return "mate";
    case PerceptFocusKind::Threat:
      return "threat";
    case PerceptFocusKind::None:
    default:
      return "none";
  }
}

void appendEscaped(std::ostream& out, const std::string& text) {
  out << '"';
  for (char ch : text) {
    if (ch == '"' || ch == '\\') {
      out << '\\';
    }
    out << ch;
  }
  out << '"';
}

}  // namespace

std::string sessionDebugLogPath(const std::string& directory) {
  std::filesystem::path path(directory);
  path /= SessionDebugLog::kDefaultFileName;
  return path.string();
}

bool SessionDebugLog::open(const std::string& directory, int intervalMs, std::uint64_t seed,
                           const char* archetypeLabel) {
  close();
  intervalMs_ = std::max(1, intervalMs);
  intervalTicks_ = std::max(1, (intervalMs_ * 60) / 1000);
  seed_ = seed;
  nextLogTick_ = 0;
  maxPopulation_ = 0;
  birthsDetected_ = 0;
  firstBirthTick_ = 0;
  firstChildId_ = 0;
  oracleId_ = 0;
  maxSeedId_ = 0;
  offspringSeen_.clear();
  headerWritten_ = false;

  path_ = sessionDebugLogPath(directory);
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(directory), ec);
  std::filesystem::remove(path_, ec);

  out_.open(path_, std::ios::out | std::ios::trunc);
  if (!out_) {
    path_.clear();
    return false;
  }

  out_ << "# evo-lab session debug log\n"
       << "# interval_ms=" << intervalMs_ << " interval_ticks=" << intervalTicks_
       << " seed=" << seed_ << " archetype=" << archetypeLabel << '\n';
  out_.flush();
  return true;
}

void SessionDebugLog::close() {
  if (out_.is_open()) {
    out_.close();
  }
}

void SessionDebugLog::writeOrganismRecord(const Organism& organism, bool oracleDetail) {
  if (!oracleDetail) {
    return;
  }

  out_ << "  org id=" << organism.id << " alive=" << (organism.alive ? 1 : 0)
       << " oracle=" << (organism.feedbagOracle ? 1 : 0)
       << " hub=" << organism.bodyStorage.size() << " P=" << nodeStoreBytes(organism, NeuronType::Perceptor)
       << " M=" << nodeStoreBytes(organism, NeuronType::Mouth)
       << " A=" << nodeStoreBytes(organism, NeuronType::Actuator) << " heading="
       << std::fixed << std::setprecision(3) << organism.heading << " offspring="
       << organism.offspringSpawnedCount << " celebration=" << organism.parthenogenesisCelebrationStartTick
       << " focus=" << focusKindLabel(organism.lastPerceptFocusKind)
       << " percept=" << static_cast<int>(organism.lastPerceptConfidence) << "/7"
       << " stroke=" << (organism.lastStrokePaid ? 1 : 0) << " disp="
       << organism.lastDisplacement << " root=(" << organism.rootWorldX() << ','
       << organism.rootWorldZ() << ")\n";
}

void SessionDebugLog::writeSnapshot(std::uint64_t simTick, const BarrenWorld& world,
                                    const EnergonField& energon, const CellPopulation& cells,
                                    float sunIntensity, const char* eventLabel) {
  if (!out_) {
    return;
  }

  const CellPopulationStats stats = cells.stats();
  const EnergonStats energonStats = computeEnergonStats(energon);

  out_ << "tick=" << simTick;
  if (eventLabel != nullptr && eventLabel[0] != '\0') {
    out_ << " event=";
    appendEscaped(out_, eventLabel);
  }
  out_ << " pop=" << stats.organisms << " camp=" << stats.campNomOrganisms
       << " degraded=" << stats.degradedNomOrganisms << " live=" << stats.liveCells
       << " sun=" << std::fixed << std::setprecision(3) << sunIntensity << " tide="
       << world.waterLevel() << " energon_blobs=" << energonStats.blobCount
       << " energon_bytes=" << energonStats.totalBytes << '\n';

  for (const Organism& organism : cells.organisms()) {
    if (organism.feedbagOracle) {
      oracleId_ = static_cast<int>(organism.id);
      writeOrganismRecord(organism, true);
    } else if (organism.offspringSpawnedCount == 0 && organism.createdAtTick > 0 &&
               simTick - organism.createdAtTick < static_cast<std::uint64_t>(intervalTicks_)) {
      writeOrganismRecord(organism, true);
    }
  }

  out_.flush();
}

void SessionDebugLog::onSimTick(std::uint64_t simTick, const BarrenWorld& world,
                                const EnergonField& energon, const CellPopulation& cells,
                                float sunIntensity) {
  if (!out_) {
    return;
  }

  const CellPopulationStats stats = cells.stats();
  maxPopulation_ = std::max(maxPopulation_, stats.organisms);

  for (const Organism& organism : cells.organisms()) {
    const std::uint32_t prev = offspringSeen_[organism.id];
    if (organism.offspringSpawnedCount > prev) {
      offspringSeen_[organism.id] = organism.offspringSpawnedCount;
      ++birthsDetected_;
      if (firstBirthTick_ == 0) {
        firstBirthTick_ = simTick;
      }

      std::ostringstream event;
      event << "birth parent=" << organism.id << " offspring_count=" << organism.offspringSpawnedCount;
      writeSnapshot(simTick, world, energon, cells, sunIntensity, event.str().c_str());
    }

    if (maxSeedId_ > 0 && organism.createdAtTick == simTick && organism.id > maxSeedId_) {
      if (firstChildId_ == 0) {
        firstChildId_ = organism.id;
      }
      std::ostringstream event;
      event << "spawn_child id=" << organism.id;
      writeSnapshot(simTick, world, energon, cells, sunIntensity, event.str().c_str());
    }
  }

  if (!headerWritten_) {
    headerWritten_ = true;
    for (const Organism& organism : cells.organisms()) {
      maxSeedId_ = std::max(maxSeedId_, organism.id);
    }
    writeSnapshot(simTick, world, energon, cells, sunIntensity, "session_start");
    nextLogTick_ = simTick + static_cast<std::uint64_t>(intervalTicks_);
    return;
  }

  if (simTick >= nextLogTick_) {
    writeSnapshot(simTick, world, energon, cells, sunIntensity, nullptr);
    nextLogTick_ = simTick + static_cast<std::uint64_t>(intervalTicks_);
  }
}

SessionDebugSummary SessionDebugLog::finalize() {
  SessionDebugSummary summary;
  summary.logOpened = out_.is_open();
  summary.maxPopulation = maxPopulation_;
  summary.birthsDetected = birthsDetected_;
  summary.firstBirthTick = firstBirthTick_;
  summary.firstChildId = firstChildId_;
  summary.oracleId = oracleId_;

  if (out_) {
    out_ << "# session_end max_pop=" << maxPopulation_ << " births=" << birthsDetected_
         << " first_birth_tick=" << firstBirthTick_ << " first_child_id=" << firstChildId_
         << " oracle_id=" << oracleId_ << '\n';
    out_.flush();
    close();
  }

  return summary;
}

SessionDebugSummary analyzeSessionDebugLog(const std::string& path) {
  SessionDebugSummary summary;
  std::ifstream in(path);
  if (!in) {
    return summary;
  }

  summary.logOpened = true;
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind("# session_end", 0) == 0) {
      summary.ok = true;
      auto parseField = [&](const char* key, auto& outValue) {
        const std::string token = std::string(key) + '=';
        const std::size_t pos = line.find(token);
        if (pos == std::string::npos) {
          return;
        }
        std::istringstream stream(line.substr(pos + token.size()));
        stream >> outValue;
      };
      parseField("max_pop", summary.maxPopulation);
      parseField("births", summary.birthsDetected);
      parseField("first_birth_tick", summary.firstBirthTick);
      parseField("first_child_id", summary.firstChildId);
      parseField("oracle_id", summary.oracleId);
      continue;
    }

    if (line.rfind("tick=", 0) == 0) {
      std::uint64_t tick = 0;
      std::istringstream stream(line.substr(5));
      stream >> tick;
      summary.finalTick = std::max(summary.finalTick, tick);
    }
  }

  if (!summary.ok && summary.finalTick > 0) {
    summary.ok = true;
  }
  return summary;
}

}  // namespace evolab
