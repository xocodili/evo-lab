#include "engine/Viewport.hpp"

#include <algorithm>

namespace evolab::engine {

ViewportLayout computeLetterbox(int drawableW, int drawableH, int designW, int designH) {
  ViewportLayout layout;
  layout.drawableW = drawableW;
  layout.drawableH = drawableH;
  layout.designW = designW;
  layout.designH = designH;

  if (drawableW <= 0 || drawableH <= 0 || designW <= 0 || designH <= 0) {
    return layout;
  }

  const float scaleX = static_cast<float>(drawableW) / static_cast<float>(designW);
  const float scaleY = static_cast<float>(drawableH) / static_cast<float>(designH);
  layout.scale = std::min(scaleX, scaleY);
  layout.contentW = static_cast<int>(static_cast<float>(designW) * layout.scale);
  layout.contentH = static_cast<int>(static_cast<float>(designH) * layout.scale);
  layout.offsetX = (drawableW - layout.contentW) / 2;
  layout.offsetY = (drawableH - layout.contentH) / 2;
  return layout;
}

void mapScreenToDesign(int screenX, int screenY, const ViewportLayout& layout, int& designX,
                       int& designY) {
  if (layout.scale <= 0.0f || layout.contentW <= 0 || layout.contentH <= 0) {
    designX = screenX;
    designY = screenY;
    return;
  }

  const float localX = static_cast<float>(screenX - layout.offsetX) / layout.scale;
  const float localY = static_cast<float>(screenY - layout.offsetY) / layout.scale;
  designX = static_cast<int>(std::clamp(localX, 0.0f, static_cast<float>(layout.designW - 1)));
  designY = static_cast<int>(std::clamp(localY, 0.0f, static_cast<float>(layout.designH - 1)));
}

}  // namespace evolab::engine
