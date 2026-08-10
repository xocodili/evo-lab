#include "engine/gfx/TextPanel.hpp"

namespace evolab::engine::gfx {

bool TextPanel::init(const UiFont& font, const std::string& sizeTemplate, TextOverlayAnchor anchor) {
  if (initialized_) {
    return true;
  }
  if (!overlay_.init(font, sizeTemplate)) {
    return false;
  }
  anchor_ = anchor;
  initialized_ = true;
  return true;
}

void TextPanel::shutdown() {
  overlay_.shutdown();
  initialized_ = false;
}

void TextPanel::draw(const std::string& text, int viewportW, int viewportH) {
  if (!initialized_ || text.empty()) {
    return;
  }
  overlay_.draw(text, viewportW, viewportH, anchor_);
}

}  // namespace evolab::engine::gfx
