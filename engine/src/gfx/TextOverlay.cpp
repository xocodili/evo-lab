#include "engine/gfx/TextOverlay.hpp"

#include "engine/gfx/ShaderProgram.hpp"
#include "engine/gl/GlContext.hpp"

#include <vector>

namespace evolab::engine::gfx {

namespace {

using gl::GlContext;

struct HudVertex {
  float x, y;
  float r, g, b, a;
};

struct TextVertex {
  float x, y;
  float u, v;
};

void pushTri(std::vector<HudVertex>& out, float x0, float y0, float x1, float y1, float x2, float y2,
             float r, float g, float b, float a) {
  out.push_back({x0, y0, r, g, b, a});
  out.push_back({x1, y1, r, g, b, a});
  out.push_back({x2, y2, r, g, b, a});
}

void pushRect(std::vector<HudVertex>& out, float x, float y, float w, float h, float r, float g,
              float b, float a) {
  pushTri(out, x, y, x + w, y, x + w, y + h, r, g, b, a);
  pushTri(out, x, y, x + w, y + h, x, y + h, r, g, b, a);
}

void pushTextQuad(std::vector<TextVertex>& out, float x, float y, float w, float h) {
  out.push_back({x, y, 0.0f, 0.0f});
  out.push_back({x + w, y, 1.0f, 0.0f});
  out.push_back({x + w, y + h, 1.0f, 1.0f});
  out.push_back({x, y, 0.0f, 0.0f});
  out.push_back({x + w, y + h, 1.0f, 1.0f});
  out.push_back({x, y + h, 0.0f, 1.0f});
}

const char* kSolidVert = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform vec2 uScreen;
out vec4 vColor;
void main() {
  vColor = aColor;
  vec2 ndc = vec2((aPos.x / uScreen.x) * 2.0 - 1.0, 1.0 - (aPos.y / uScreen.y) * 2.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

const char* kSolidFrag = R"(#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
  FragColor = vColor;
}
)";

const char* kTextVert = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
uniform vec2 uScreen;
out vec2 vUv;
void main() {
  vUv = aUv;
  vec2 ndc = vec2((aPos.x / uScreen.x) * 2.0 - 1.0, 1.0 - (aPos.y / uScreen.y) * 2.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

const char* kTextFrag = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
  FragColor = texture(uTex, vUv);
}
)";

}  // namespace

TextOverlay::TextOverlay() = default;

TextOverlay::~TextOverlay() { shutdown(); }

bool TextOverlay::init(const UiFont& font, const std::string& sizeTemplate) {
  if (initialized_) {
    return true;
  }
  if (!font.loaded()) {
    return false;
  }

  font_ = &font;

  std::vector<unsigned char> sizingBitmap;
  if (!font.renderTextBitmap(sizeTemplate, sizingBitmap, atlasW_, atlasH_)) {
    font_ = nullptr;
    return false;
  }

  constexpr float kPadX = 16.0f;
  constexpr float kPadY = 14.0f;
  panelW_ = static_cast<float>(atlasW_) + kPadX * 2.0f;
  panelH_ = static_cast<float>(atlasH_) + kPadY * 2.0f;

  solidProgram_ = ShaderProgram::create(kSolidVert, kSolidFrag);
  textProgram_ = ShaderProgram::create(kTextVert, kTextFrag);

  GlContext& g = gl::gl();
  g.genVertexArrays(1, &vao_);
  g.genBuffers(1, &vbo_);
  g.genTextures(1, &textTexture_);

  initialized_ = true;
  return true;
}

void TextOverlay::shutdown() {
  if (!initialized_) {
    return;
  }

  GlContext& g = gl::gl();
  font_ = nullptr;
  if (textTexture_) {
    g.deleteTextures(1, &textTexture_);
    textTexture_ = 0;
  }
  if (vbo_) {
    g.deleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (vao_) {
    g.deleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  solidProgram_ = ShaderProgram{};
  textProgram_ = ShaderProgram{};
  initialized_ = false;
}

void TextOverlay::draw(const std::string& text, int viewportW, int viewportH, TextOverlayAnchor anchor) {
  if (!initialized_ || viewportW <= 0 || viewportH <= 0 || font_ == nullptr) {
    return;
  }

  std::vector<unsigned char> rgba;
  int textW = atlasW_;
  int textH = atlasH_;
  if (!font_->renderTextBitmap(text, rgba, textW, textH, atlasW_, atlasH_)) {
    return;
  }

  GlContext& g = gl::gl();
  g.bindTexture(gl::GlEnum::kTexture2D, textTexture_);
  g.texParameteri(gl::GlEnum::kTexture2D, gl::GlEnum::kTextureMinFilter, gl::GlEnum::kLinear);
  g.texParameteri(gl::GlEnum::kTexture2D, gl::GlEnum::kTextureMagFilter, gl::GlEnum::kLinear);
  g.texImage2D(gl::GlEnum::kTexture2D, 0, static_cast<int>(gl::GlEnum::kRgba), textW, textH, 0,
               gl::GlEnum::kRgba, gl::GlEnum::kUnsignedByte, rgba.data());

  constexpr float kMargin = 14.0f;
  constexpr float kPadX = 16.0f;
  constexpr float kPadY = 14.0f;
  float panelX = static_cast<float>(viewportW) - panelW_ - kMargin;
  float panelY = kMargin;
  if (anchor == TextOverlayAnchor::BottomLeft) {
    panelX = kMargin;
    panelY = static_cast<float>(viewportH) - panelH_ - kMargin;
  }

  g.disable(gl::GlEnum::kDepthTest);
  g.enable(gl::GlEnum::kBlend);
  g.blendFunc(gl::GlEnum::kSrcAlpha, gl::GlEnum::kOneMinusSrcAlpha);
  g.viewport(0, 0, viewportW, viewportH);

  std::vector<HudVertex> solidVerts;
  solidVerts.reserve(12);
  pushRect(solidVerts, panelX, panelY, panelW_, panelH_, 0.04f, 0.06f, 0.10f, 0.88f);
  pushRect(solidVerts, panelX, panelY, panelW_, 3.0f, 0.35f, 0.65f, 0.95f, 0.95f);

  g.bindVertexArray(vao_);
  g.bindBuffer(gl::GlEnum::kArrayBuffer, vbo_);
  g.bufferData(gl::GlEnum::kArrayBuffer, static_cast<gl::GLsizeiptr>(solidVerts.size() * sizeof(HudVertex)),
               solidVerts.data(), gl::GlEnum::kDynamicDraw);
  g.enableVertexAttribArray(0);
  g.vertexAttribPointer(0, 2, gl::GlEnum::kFloat, gl::GlEnum::kFalse, sizeof(HudVertex),
                        reinterpret_cast<void*>(0));
  g.enableVertexAttribArray(1);
  g.vertexAttribPointer(1, 4, gl::GlEnum::kFloat, gl::GlEnum::kFalse, sizeof(HudVertex),
                        reinterpret_cast<void*>(2 * sizeof(float)));

  g.useProgram(solidProgram_.id());
  g.uniform2f(g.getUniformLocation(solidProgram_.id(), "uScreen"), static_cast<float>(viewportW),
              static_cast<float>(viewportH));
  g.drawArrays(gl::GlEnum::kTriangles, 0, static_cast<int>(solidVerts.size()));

  std::vector<TextVertex> textVerts;
  textVerts.reserve(6);
  const float contentW = static_cast<float>(textW);
  const float contentH = static_cast<float>(textH);
  pushTextQuad(textVerts, panelX + kPadX, panelY + kPadY, contentW, contentH);

  g.bufferData(gl::GlEnum::kArrayBuffer, static_cast<gl::GLsizeiptr>(textVerts.size() * sizeof(TextVertex)),
               textVerts.data(), gl::GlEnum::kDynamicDraw);
  g.enableVertexAttribArray(0);
  g.vertexAttribPointer(0, 2, gl::GlEnum::kFloat, gl::GlEnum::kFalse, sizeof(TextVertex),
                        reinterpret_cast<void*>(0));
  g.enableVertexAttribArray(1);
  g.vertexAttribPointer(1, 2, gl::GlEnum::kFloat, gl::GlEnum::kFalse, sizeof(TextVertex),
                        reinterpret_cast<void*>(2 * sizeof(float)));

  g.useProgram(textProgram_.id());
  g.uniform2f(g.getUniformLocation(textProgram_.id(), "uScreen"), static_cast<float>(viewportW),
              static_cast<float>(viewportH));
  g.activeTexture(gl::GlEnum::kTexture0);
  g.bindTexture(gl::GlEnum::kTexture2D, textTexture_);
  g.uniform1i(g.getUniformLocation(textProgram_.id(), "uTex"), 0);
  g.drawArrays(gl::GlEnum::kTriangles, 0, static_cast<int>(textVerts.size()));

  g.bindVertexArray(0);
  g.enable(gl::GlEnum::kDepthTest);
}

}  // namespace evolab::engine::gfx
