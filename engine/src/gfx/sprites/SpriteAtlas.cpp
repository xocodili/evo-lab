#include "engine/gfx/sprites/SpriteAtlas.hpp"

#include "engine/gl/GlContext.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace evolab::engine::gfx::sprites {

namespace {

std::string readFileToString(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return {};
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void skipWs(const std::string& text, std::size_t& i) {
  while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) != 0) {
    ++i;
  }
}

bool matchLiteral(const std::string& text, std::size_t& i, char literal) {
  skipWs(text, i);
  if (i >= text.size() || text[i] != literal) {
    return false;
  }
  ++i;
  return true;
}

bool parseString(const std::string& text, std::size_t& i, std::string& out) {
  skipWs(text, i);
  if (i >= text.size() || text[i] != '"') {
    return false;
  }
  ++i;
  out.clear();
  while (i < text.size()) {
    const char c = text[i++];
    if (c == '"') {
      return true;
    }
    if (c == '\\' && i < text.size()) {
      out.push_back(text[i++]);
      continue;
    }
    out.push_back(c);
  }
  return false;
}

bool parseNumber(const std::string& text, std::size_t& i, float& out) {
  skipWs(text, i);
  std::size_t start = i;
  while (i < text.size() &&
         (std::isdigit(static_cast<unsigned char>(text[i])) != 0 || text[i] == '.' || text[i] == '-' ||
          text[i] == '+' || text[i] == 'e' || text[i] == 'E')) {
    ++i;
  }
  if (start == i) {
    return false;
  }
  out = std::stof(text.substr(start, i - start));
  return true;
}

bool parseIntArray(const std::string& text, std::size_t& i, std::vector<int>& out) {
  if (!matchLiteral(text, i, '[')) {
    return false;
  }
  out.clear();
  skipWs(text, i);
  if (i < text.size() && text[i] == ']') {
    ++i;
    return true;
  }
  while (i < text.size()) {
    float value = 0.0f;
    if (!parseNumber(text, i, value)) {
      return false;
    }
    out.push_back(static_cast<int>(value));
    skipWs(text, i);
    if (i < text.size() && text[i] == ',') {
      ++i;
      continue;
    }
    break;
  }
  return matchLiteral(text, i, ']');
}

bool parseFramesMatrix(const std::string& text, std::size_t& i,
                       std::vector<SpriteFrame>& frames) {
  if (!matchLiteral(text, i, '[')) {
    return false;
  }
  frames.clear();
  skipWs(text, i);
  if (i < text.size() && text[i] == ']') {
    ++i;
    return true;
  }
  while (i < text.size()) {
    std::vector<int> rect;
    if (!parseIntArray(text, i, rect) || rect.size() != 4) {
      return false;
    }
    SpriteFrame frame;
    frame.x = rect[0];
    frame.y = rect[1];
    frame.w = rect[2];
    frame.h = rect[3];
    frames.push_back(frame);
    skipWs(text, i);
    if (i < text.size() && text[i] == ',') {
      ++i;
      continue;
    }
    break;
  }
  return matchLiteral(text, i, ']');
}

SpriteLoopMode parseLoopMode(const std::string& value) {
  if (value == "once") {
    return SpriteLoopMode::Once;
  }
  if (value == "pingpong") {
    return SpriteLoopMode::PingPong;
  }
  if (value == "hold") {
    return SpriteLoopMode::HoldLast;
  }
  return SpriteLoopMode::Loop;
}

bool parseClipObject(const std::string& text, std::size_t& i, SpriteClip& clip) {
  if (!matchLiteral(text, i, '{')) {
    return false;
  }
  while (i < text.size()) {
    skipWs(text, i);
    if (i < text.size() && text[i] == '}') {
      ++i;
      return true;
    }
    std::string key;
    if (!parseString(text, i, key) || !matchLiteral(text, i, ':')) {
      return false;
    }
    if (key == "fps") {
      parseNumber(text, i, clip.defaultFps);
    } else if (key == "loop") {
      std::string mode;
      if (!parseString(text, i, mode)) {
        return false;
      }
      clip.loop = parseLoopMode(mode);
    } else if (key == "frames") {
      if (!parseFramesMatrix(text, i, clip.frames)) {
        return false;
      }
    } else {
      skipWs(text, i);
      if (i < text.size() && text[i] == '"') {
        std::string ignored;
        if (!parseString(text, i, ignored)) {
          return false;
        }
      } else if (i < text.size() && text[i] == '[') {
        std::size_t depth = 0;
        do {
          if (text[i] == '[') {
            ++depth;
          } else if (text[i] == ']') {
            --depth;
          }
          ++i;
        } while (i < text.size() && depth > 0);
      } else if (i < text.size() && text[i] == '{') {
        std::size_t depth = 0;
        do {
          if (text[i] == '{') {
            ++depth;
          } else if (text[i] == '}') {
            --depth;
          }
          ++i;
        } while (i < text.size() && depth > 0);
      } else {
        float ignored = 0.0f;
        parseNumber(text, i, ignored);
      }
    }
    skipWs(text, i);
    if (i < text.size() && text[i] == ',') {
      ++i;
    }
  }
  return false;
}

bool parseClipsObject(const std::string& text, std::size_t& i,
                      std::unordered_map<std::string, SpriteClip>& clips) {
  if (!matchLiteral(text, i, '{')) {
    return false;
  }
  clips.clear();
  while (i < text.size()) {
    skipWs(text, i);
    if (i < text.size() && text[i] == '}') {
      ++i;
      return true;
    }
    std::string clipName;
    if (!parseString(text, i, clipName) || !matchLiteral(text, i, ':')) {
      return false;
    }
    SpriteClip clip;
    clip.name = clipName;
    if (!parseClipObject(text, i, clip)) {
      return false;
    }
    clips.emplace(clipName, std::move(clip));
    skipWs(text, i);
    if (i < text.size() && text[i] == ',') {
      ++i;
    }
  }
  return false;
}

std::string dirnameOf(const std::string& path) {
  const std::size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) {
    return {};
  }
  return path.substr(0, pos);
}

std::string joinPath(const std::string& dir, const std::string& file) {
  if (dir.empty()) {
    return file;
  }
  if (dir.back() == '/' || dir.back() == '\\') {
    return dir + file;
  }
  return dir + "/" + file;
}

}  // namespace

SpriteUvRect SpriteAtlas::uvForFrame(const SpriteFrame& frame) const {
  SpriteUvRect uv;
  if (widthPx <= 0 || heightPx <= 0) {
    return uv;
  }
  uv.u0 = static_cast<float>(frame.x) / static_cast<float>(widthPx);
  uv.v0 = static_cast<float>(frame.y) / static_cast<float>(heightPx);
  uv.u1 = static_cast<float>(frame.x + frame.w) / static_cast<float>(widthPx);
  uv.v1 = static_cast<float>(frame.y + frame.h) / static_cast<float>(heightPx);
  return uv;
}

const SpriteClip* SpriteAtlas::findClip(const std::string& name) const {
  const auto it = clips.find(name);
  if (it == clips.end()) {
    return nullptr;
  }
  return &it->second;
}

bool loadSpriteManifest(const std::string& manifestPath, SpriteAtlas& outAtlas) {
  const std::string text = readFileToString(manifestPath);
  if (text.empty()) {
    return false;
  }

  outAtlas.clips.clear();
  std::size_t i = 0;
  if (!matchLiteral(text, i, '{')) {
    return false;
  }

  std::string imageName;
  while (i < text.size()) {
    skipWs(text, i);
    if (i < text.size() && text[i] == '}') {
      break;
    }
    std::string key;
    if (!parseString(text, i, key) || !matchLiteral(text, i, ':')) {
      return false;
    }
    if (key == "image") {
      if (!parseString(text, i, imageName)) {
        return false;
      }
    } else if (key == "clips") {
      if (!parseClipsObject(text, i, outAtlas.clips)) {
        return false;
      }
    } else {
      skipWs(text, i);
      if (i < text.size() && text[i] == '"') {
        std::string ignored;
        if (!parseString(text, i, ignored)) {
          return false;
        }
      } else if (i < text.size() && (text[i] == '{' || text[i] == '[')) {
        const char open = text[i];
        const char close = open == '{' ? '}' : ']';
        int depth = 0;
        do {
          if (text[i] == open) {
            ++depth;
          } else if (text[i] == close) {
            --depth;
          }
          ++i;
        } while (i < text.size() && depth > 0);
      } else {
        float ignored = 0.0f;
        parseNumber(text, i, ignored);
      }
    }
    skipWs(text, i);
    if (i < text.size() && text[i] == ',') {
      ++i;
    }
  }

  if (imageName.empty()) {
    return false;
  }
  outAtlas.imagePath = joinPath(dirnameOf(manifestPath), imageName);
  return !outAtlas.clips.empty();
}

bool loadSpriteAtlasImage(const std::string& imagePath, SpriteAtlas& atlas) {
  stbi_set_flip_vertically_on_load(1);
  int w = 0;
  int h = 0;
  int comp = 0;
  stbi_uc* pixels =
      stbi_load(imagePath.c_str(), &w, &h, &comp, STBI_rgb_alpha);
  stbi_set_flip_vertically_on_load(0);
  if (pixels == nullptr || w <= 0 || h <= 0) {
    return false;
  }

  atlas.widthPx = w;
  atlas.heightPx = h;
  atlas.cpuPixels.assign(pixels, pixels + static_cast<std::size_t>(w * h * 4));
  stbi_image_free(pixels);
  return true;
}

bool uploadSpriteAtlasTexture(SpriteAtlas& atlas) {
  if (atlas.cpuPixels.empty() || atlas.widthPx <= 0 || atlas.heightPx <= 0) {
    return false;
  }

  engine::gl::GlContext& g = engine::gl::gl();
  if (!g.loaded) {
    return false;
  }

  if (atlas.textureId == 0) {
    g.genTextures(1, &atlas.textureId);
  }

  g.bindTexture(engine::gl::GlEnum::kTexture2D, atlas.textureId);
  g.texParameteri(engine::gl::GlEnum::kTexture2D, engine::gl::GlEnum::kTextureMinFilter,
                  engine::gl::GlEnum::kLinear);
  g.texParameteri(engine::gl::GlEnum::kTexture2D, engine::gl::GlEnum::kTextureMagFilter,
                  engine::gl::GlEnum::kLinear);
  g.texImage2D(engine::gl::GlEnum::kTexture2D, 0, engine::gl::GlEnum::kRgba, atlas.widthPx,
               atlas.heightPx, 0, engine::gl::GlEnum::kRgba, engine::gl::GlEnum::kUnsignedByte,
               atlas.cpuPixels.data());
  g.bindTexture(engine::gl::GlEnum::kTexture2D, 0);
  return true;
}

}  // namespace evolab::engine::gfx::sprites
