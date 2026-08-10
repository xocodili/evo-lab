#include "engine/gfx/UiFont.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

namespace evolab::engine::gfx {

namespace {

constexpr float kFontDpi = 96.0f;

struct LoadedFont {
  std::vector<unsigned char> bytes;
  stbtt_fontinfo info{};
  float scale = 0.0f;
  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  bool ok = false;
};

bool isTrueTypeFont(const std::vector<unsigned char>& bytes) {
  if (bytes.size() < 4) {
    return false;
  }
  const unsigned tag =
      (static_cast<unsigned>(bytes[0]) << 24) | (static_cast<unsigned>(bytes[1]) << 16) |
      (static_cast<unsigned>(bytes[2]) << 8) | static_cast<unsigned>(bytes[3]);
  return tag == 0x00010000u || tag == 0x4F54544Fu || tag == 0x74727565u || tag == 0x74797031u;
}

bool loadFontFile(LoadedFont& font, const std::string& path, float pointSize) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::cerr << "UiFont not found: " << path << '\n';
    return false;
  }
  input.seekg(0, std::ios::end);
  const std::streamsize size = input.tellg();
  input.seekg(0, std::ios::beg);
  if (size <= 0) {
    return false;
  }
  font.bytes.resize(static_cast<std::size_t>(size));
  if (!input.read(reinterpret_cast<char*>(font.bytes.data()), size)) {
    return false;
  }
  if (!isTrueTypeFont(font.bytes)) {
    std::cerr << "UiFont is not a valid TTF/OTF file: " << path << '\n';
    return false;
  }

  const int offset = stbtt_GetFontOffsetForIndex(font.bytes.data(), 0);
  if (offset < 0 || !stbtt_InitFont(&font.info, font.bytes.data(), offset)) {
    return false;
  }

  const float pixelHeight = pointSize * (kFontDpi / 72.0f);
  font.scale = stbtt_ScaleForPixelHeight(&font.info, pixelHeight);
  stbtt_GetFontVMetrics(&font.info, &font.ascent, &font.descent, &font.lineGap);
  font.ok = true;
  return true;
}

int measureLineWidth(const LoadedFont& font, const std::string& line) {
  int width = 0;
  for (unsigned char ch : line) {
    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&font.info, ch, &advance, &lsb);
    width += static_cast<int>(advance * font.scale);
  }
  return width;
}

void blitGlyph(std::vector<unsigned char>& rgba, int atlasW, int atlasH, int dstX, int dstY,
               unsigned char* bitmap, int gw, int gh, unsigned char r, unsigned char g,
               unsigned char b) {
  for (int y = 0; y < gh; ++y) {
    for (int x = 0; x < gw; ++x) {
      const int px = dstX + x;
      const int py = dstY + y;
      if (px < 0 || py < 0 || px >= atlasW || py >= atlasH) {
        continue;
      }
      const unsigned char alpha = bitmap[y * gw + x];
      if (alpha == 0) {
        continue;
      }
      unsigned char* pixel = rgba.data() + static_cast<std::size_t>((py * atlasW + px) * 4);
      pixel[0] = r;
      pixel[1] = g;
      pixel[2] = b;
      pixel[3] = alpha;
    }
  }
}

}  // namespace

struct UiFont::State {
  LoadedFont font;
};

UiFont::UiFont() = default;

UiFont::~UiFont() {
  delete state_;
  state_ = nullptr;
}

bool UiFont::load(const std::string& path, float pointSize) {
  delete state_;
  state_ = new State();
  return loadFontFile(state_->font, path, pointSize);
}

bool UiFont::loaded() const { return state_ != nullptr && state_->font.ok; }

bool UiFont::renderTextBitmap(const std::string& text, std::vector<unsigned char>& rgba, int& outW,
                              int& outH, int minW, int minH) const {
  if (!loaded()) {
    return false;
  }

  const LoadedFont& font = state_->font;

  std::vector<std::string> lines;
  {
    std::string current;
    for (char ch : text) {
      if (ch == '\n') {
        lines.push_back(current);
        current.clear();
      } else {
        current.push_back(ch);
      }
    }
    lines.push_back(current);
  }

  const float lineAdvance =
      font.scale * static_cast<float>(font.ascent - font.descent + font.lineGap);
  int maxLineW = 0;
  for (const std::string& line : lines) {
    maxLineW = std::max(maxLineW, measureLineWidth(font, line));
  }

  constexpr int kPad = 8;
  outW = std::max(maxLineW + kPad * 2, minW);
  outH = std::max(static_cast<int>(lines.size() * lineAdvance) + kPad * 2, minH);
  if (outW <= 0 || outH <= 0) {
    return false;
  }

  rgba.assign(static_cast<std::size_t>(outW * outH * 4), 0);
  const unsigned char textR = 235;
  const unsigned char textG = 244;
  const unsigned char textB = 255;

  int baselineY = kPad + static_cast<int>(font.ascent * font.scale);
  for (const std::string& line : lines) {
    int penX = kPad;
    for (unsigned char ch : line) {
      int advance = 0;
      int lsb = 0;
      stbtt_GetCodepointHMetrics(&font.info, ch, &advance, &lsb);

      int x0 = 0;
      int y0 = 0;
      int x1 = 0;
      int y1 = 0;
      stbtt_GetCodepointBitmapBox(&font.info, ch, font.scale, font.scale, &x0, &y0, &x1, &y1);
      int gw = 0;
      int gh = 0;
      unsigned char* bitmap =
          stbtt_GetCodepointBitmap(&font.info, 0, font.scale, ch, &gw, &gh, 0, 0);
      if (bitmap) {
        blitGlyph(rgba, outW, outH, penX + x0, baselineY + y0, bitmap, gw, gh, textR, textG, textB);
        stbtt_FreeBitmap(bitmap, nullptr);
      }
      penX += static_cast<int>(advance * font.scale);
    }
    baselineY += static_cast<int>(lineAdvance);
  }

  return true;
}

}  // namespace evolab::engine::gfx
