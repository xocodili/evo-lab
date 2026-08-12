#pragma once

#include "engine/gfx/UiFont.hpp"

#include <string>

namespace evolab::engine::gfx {

enum class TextOverlayAnchor {
  TopRight,
  BottomLeft,
};

class TextOverlay {
public:
  TextOverlay();
  ~TextOverlay();

  TextOverlay(const TextOverlay&) = delete;
  TextOverlay& operator=(const TextOverlay&) = delete;

  bool init(const UiFont& font, const std::string& sizeTemplate);
  void shutdown();
  void draw(const std::string& text, int viewportW, int viewportH,
            TextOverlayAnchor anchor = TextOverlayAnchor::TopRight);

private:
  bool initialized_ = false;
  const UiFont* font_ = nullptr;
  unsigned textTexture_ = 0;
  int atlasW_ = 0;
  int atlasH_ = 0;
  float panelW_ = 0.0f;
  float panelH_ = 0.0f;
};

}  // namespace evolab::engine::gfx
