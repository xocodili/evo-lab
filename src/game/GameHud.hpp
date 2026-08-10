#pragma once

#include "engine/gfx/TextPanel.hpp"
#include "engine/gfx/UiFont.hpp"

#include "sim/SimDiagnostics.hpp"

#include <string>

namespace evolab::game {

class GameHud {
public:
  bool init(const engine::gfx::UiFont& font);
  void shutdown();
  void draw(const SimDiagnostics& stats, int viewportW, int viewportH);

private:
  engine::gfx::TextPanel panel_;
};

std::string formatDiagnosticsText(const SimDiagnostics& stats);

}  // namespace evolab::game
