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

struct CachedGlyph {
  int advance = 0;
  int x0 = 0;
  int y0 = 0;
  int w = 0;
  int h = 0;
  std::vector<unsigned char> alpha;
  bool valid = false;
};

struct LoadedFont {
  std::vector<unsigned char> bytes;
  stbtt_fontinfo info{};
  float scale = 0.0f;
  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  bool ok = false;
  std::vector<CachedGlyph> glyphCache;
};

void buildGlyphCache(LoadedFont& font);
const CachedGlyph* lookupGlyph(const LoadedFont& font, unsigned char ch);
int measureCodepointWidth(const LoadedFont& font, int codepoint);
void drawCodepoint(const LoadedFont& font, std::vector<unsigned char>& rgba, int atlasW, int atlasH,
                   int penX, int baselineY, int codepoint, unsigned char textR,
                   unsigned char textG, unsigned char textB);

int nextUtf8Codepoint(const std::string& text, std::size_t& index) {
  if (index >= text.size()) {
    return -1;
  }
  const unsigned char lead = static_cast<unsigned char>(text[index]);
  if (lead < 0x80) {
    ++index;
    return static_cast<int>(lead);
  }
  if ((lead & 0xE0) == 0xC0 && index + 1 < text.size()) {
    const int codepoint = static_cast<int>((lead & 0x1F) << 6) |
                          static_cast<int>(text[index + 1] & 0x3F);
    index += 2;
    return codepoint;
  }
  if ((lead & 0xF0) == 0xE0 && index + 2 < text.size()) {
    const int codepoint = static_cast<int>((lead & 0x0F) << 12) |
                          static_cast<int>((text[index + 1] & 0x3F) << 6) |
                          static_cast<int>(text[index + 2] & 0x3F);
    index += 3;
    return codepoint;
  }
  if ((lead & 0xF8) == 0xF0 && index + 3 < text.size()) {
    const int codepoint = static_cast<int>((lead & 0x07) << 18) |
                          static_cast<int>((text[index + 1] & 0x3F) << 12) |
                          static_cast<int>((text[index + 2] & 0x3F) << 6) |
                          static_cast<int>(text[index + 3] & 0x3F);
    index += 4;
    return codepoint;
  }
  ++index;
  return static_cast<int>(lead);
}

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
  buildGlyphCache(font);
  return true;
}

int measureLineWidth(const LoadedFont& font, const std::string& line) {
  int width = 0;
  for (std::size_t index = 0; index < line.size();) {
    const int codepoint = nextUtf8Codepoint(line, index);
    if (codepoint < 0) {
      break;
    }
    width += measureCodepointWidth(font, codepoint);
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

void blitCachedGlyph(std::vector<unsigned char>& rgba, int atlasW, int atlasH, int dstX, int dstY,
                     const CachedGlyph& glyph, unsigned char r, unsigned char g, unsigned char b) {
  if (!glyph.valid || glyph.w <= 0 || glyph.h <= 0) {
    return;
  }
  blitGlyph(rgba, atlasW, atlasH, dstX, dstY, const_cast<unsigned char*>(glyph.alpha.data()),
            glyph.w, glyph.h, r, g, b);
}

void buildGlyphCache(LoadedFont& font) {
  font.glyphCache.assign(128, {});
  for (int codepoint = 32; codepoint < 127; ++codepoint) {
    CachedGlyph& glyph = font.glyphCache[static_cast<std::size_t>(codepoint)];
    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&font.info, codepoint, &advance, &lsb);
    glyph.advance = static_cast<int>(advance * font.scale);

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetCodepointBitmapBox(&font.info, codepoint, font.scale, font.scale, &x0, &y0, &x1, &y1);
    glyph.x0 = x0;
    glyph.y0 = y0;

    int gw = 0;
    int gh = 0;
    unsigned char* bitmap =
        stbtt_GetCodepointBitmap(&font.info, 0, font.scale, codepoint, &gw, &gh, 0, 0);
    glyph.w = gw;
    glyph.h = gh;
    if (bitmap != nullptr && gw > 0 && gh > 0) {
      glyph.alpha.assign(bitmap, bitmap + static_cast<std::size_t>(gw * gh));
    }
    if (bitmap != nullptr) {
      stbtt_FreeBitmap(bitmap, nullptr);
    }
    glyph.valid = true;
  }
}

const CachedGlyph* lookupGlyph(const LoadedFont& font, unsigned char ch) {
  if (font.glyphCache.size() < 128 || ch >= 128) {
    return nullptr;
  }
  const CachedGlyph& glyph = font.glyphCache[static_cast<std::size_t>(ch)];
  return glyph.valid ? &glyph : nullptr;
}

int measureCodepointWidth(const LoadedFont& font, int codepoint) {
  if (codepoint >= 0 && codepoint < 128) {
    if (const CachedGlyph* glyph = lookupGlyph(font, static_cast<unsigned char>(codepoint))) {
      return glyph->advance;
    }
  }
  int advance = 0;
  int lsb = 0;
  stbtt_GetCodepointHMetrics(&font.info, codepoint, &advance, &lsb);
  return static_cast<int>(advance * font.scale);
}

void drawCodepoint(const LoadedFont& font, std::vector<unsigned char>& rgba, int atlasW, int atlasH,
                   int penX, int baselineY, int codepoint, unsigned char textR,
                   unsigned char textG, unsigned char textB) {
  if (codepoint >= 0 && codepoint < 128) {
    if (const CachedGlyph* glyph = lookupGlyph(font, static_cast<unsigned char>(codepoint))) {
      blitCachedGlyph(rgba, atlasW, atlasH, penX + glyph->x0, baselineY + glyph->y0, *glyph, textR,
                      textG, textB);
      return;
    }
  }

  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
  stbtt_GetCodepointBitmapBox(&font.info, codepoint, font.scale, font.scale, &x0, &y0, &x1, &y1);
  int gw = 0;
  int gh = 0;
  unsigned char* bitmap =
      stbtt_GetCodepointBitmap(&font.info, 0, font.scale, codepoint, &gw, &gh, 0, 0);
  if (bitmap != nullptr) {
    blitGlyph(rgba, atlasW, atlasH, penX + x0, baselineY + y0, bitmap, gw, gh, textR, textG, textB);
    stbtt_FreeBitmap(bitmap, nullptr);
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
    for (std::size_t index = 0; index < line.size();) {
      const int codepoint = nextUtf8Codepoint(line, index);
      if (codepoint < 0) {
        break;
      }
      drawCodepoint(font, rgba, outW, outH, penX, baselineY, codepoint, textR, textG, textB);
      penX += measureCodepointWidth(font, codepoint);
    }
    baselineY += static_cast<int>(lineAdvance);
  }

  return true;
}

}  // namespace evolab::engine::gfx
