#pragma once

#include "engine/gfx/ShaderProgram.hpp"
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
  ShaderProgram solidProgram_;
  ShaderProgram textProgram_;
  unsigned vao_ = 0;
  unsigned vbo_ = 0;
  unsigned textTexture_ = 0;
  int atlasW_ = 0;
  int atlasH_ = 0;
  float panelW_ = 0.0f;
  float panelH_ = 0.0f;
};

}  // namespace evolab::engine::gfx
