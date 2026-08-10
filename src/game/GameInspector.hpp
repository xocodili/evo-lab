#pragma once

#include "engine/gfx/TextPanel.hpp"
#include "engine/gfx/UiFont.hpp"

#include <string>

namespace evolab::game {

class GameInspector {
public:
  bool init(const engine::gfx::UiFont& font);
  void shutdown();
  void draw(const std::string& text, int viewportW, int viewportH);

private:
  engine::gfx::TextPanel panel_;
};

}  // namespace evolab::game
