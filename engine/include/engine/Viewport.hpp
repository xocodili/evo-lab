#pragma once

namespace evolab::engine {

struct ViewportLayout {
  int drawableW = 0;
  int drawableH = 0;
  int designW = 0;
  int designH = 0;
  float scale = 1.0f;
  int offsetX = 0;
  int offsetY = 0;
  int contentW = 0;
  int contentH = 0;
};

ViewportLayout computeLetterbox(int drawableW, int drawableH, int designW, int designH);
void mapScreenToDesign(int screenX, int screenY, const ViewportLayout& layout, int& designX,
                       int& designY);

}  // namespace evolab::engine
