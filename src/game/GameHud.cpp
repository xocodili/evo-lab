#include "game/GameHud.hpp"

#include <cstdio>

namespace evolab::game {

namespace {

const char* kHudSizeTemplate =
    "evo-lab  Phase 2.x\n"
    "Archetype: CAMP Nom\n"
    "FPS: 9999.9\n"
    "Time: Night 23:59  sun 100%\n"
    "Tide: +99.9  Falling  [-99..+99]\n"
    "Energon: 9999 blobs  99999999 bytes\n"
    "  wet 9999  dry 9999  falling 9999\n"
    "  cap 9999 / 9999\n"
    "Cells: 999 CAMP Noms (999 degraded, 999 stem)\n"
    "Hover: Nom #99999 CAMP\n"
    "Tick: 99999999  Seed: 99999999\n"
    "[Paused]";

}  // namespace

std::string formatDiagnosticsText(const SimDiagnostics& stats) {
  char buffer[1024];
  const char* hoverLine = stats.hoveredCellSummary.empty() ? "Hover: —" : stats.hoveredCellSummary.c_str();
  std::snprintf(
      buffer, sizeof(buffer),
      "evo-lab  Phase 2.x\n"
      "Archetype: %s\n"
      "FPS: %.1f\n"
      "Time: %s %02d:%02d  sun %.0f%%\n"
      "Tide: %+.1f  %s  [%.0f..%.0f]\n"
      "Energon: %d blobs  %llu bytes\n"
      "  wet %d  dry %d  falling %d\n"
      "  cap %d / %d\n"
      "Cells: %d CAMP Noms (%d degraded, %d stem)\n"
      "%s\n"
      "Tick: %llu  Seed: %llu\n"
      "%s",
      stats.archetypeLabel, stats.fps, stats.dayNight, stats.clockHours,
      stats.clockMinutes, stats.sunIntensity * 100.0f,
      stats.waterLevel, stats.tidePhase, stats.tideMin, stats.tideMax, stats.energon.blobCount,
      static_cast<unsigned long long>(stats.energon.totalBytes), stats.energon.groundedWet,
      stats.energon.groundedDry, stats.energon.falling, stats.energon.blobCount, stats.energonCap,
      stats.campNomOrganisms, stats.degradedNomOrganisms, stats.stemCells, hoverLine,
      static_cast<unsigned long long>(stats.simTick),
      static_cast<unsigned long long>(stats.seed), stats.paused ? "[Paused]" : "");
  return buffer;
}

bool GameHud::init(const engine::gfx::UiFont& font) {
  return panel_.init(font, kHudSizeTemplate, engine::gfx::TextOverlayAnchor::TopRight);
}

void GameHud::shutdown() { panel_.shutdown(); }

void GameHud::draw(const SimDiagnostics& stats, int viewportW, int viewportH) {
  panel_.draw(formatDiagnosticsText(stats), viewportW, viewportH);
}

}  // namespace evolab::game
