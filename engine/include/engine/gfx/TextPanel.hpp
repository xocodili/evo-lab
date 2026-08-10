#pragma once

#include "engine/gfx/TextOverlay.hpp"
#include "engine/gfx/UiFont.hpp"

#include <string>

namespace evolab::engine::gfx {

// Fixed-size labeled panel for HUD / inspector overlays. Rendering lives in the engine;
// game code supplies text content and layout templates only.
class TextPanel {
public:
  bool init(const UiFont& font, const std::string& sizeTemplate,
            TextOverlayAnchor anchor = TextOverlayAnchor::TopRight);
  void shutdown();

  void draw(const std::string& text, int viewportW, int viewportH);
  bool initialized() const { return initialized_; }

private:
  TextOverlay overlay_;
  TextOverlayAnchor anchor_ = TextOverlayAnchor::TopRight;
  bool initialized_ = false;
};

}  // namespace evolab::engine::gfx
