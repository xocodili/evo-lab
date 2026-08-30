#include "engine/gfx/sprites/SpriteRenderer.hpp"

#include "engine/gfx/sprites/SpriteAnimator.hpp"
#include "engine/gfx/sprites/SpriteBillboardBuilder.hpp"
#include "engine/gl/GlContext.hpp"

#include <algorithm>

namespace evolab::engine::gfx::sprites {

namespace {

const char* kVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aLocal;
layout(location = 3) in vec2 aUv;
uniform mat4 uMvp;
out vec4 vColor;
out vec2 vUv;
void main() {
  vColor = aColor;
  vUv = aUv;
  gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kFrag = R"(#version 330 core
in vec4 vColor;
in vec2 vUv;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
  vec4 tex = texture(uTex, vUv);
  if (tex.a < 0.05) {
    discard;
  }
  FragColor = tex * vColor;
}
)";

void configureSpriteVertexLayout(engine::gl::GlContext& g, unsigned vao, unsigned vbo) {
  g.bindVertexArray(vao);
  g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, vbo);
  g.enableVertexAttribArray(0);
  g.vertexAttribPointer(0, 3, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                        sizeof(SpriteVertex), reinterpret_cast<void*>(0));
  g.enableVertexAttribArray(1);
  g.vertexAttribPointer(1, 4, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                        sizeof(SpriteVertex), reinterpret_cast<void*>(3 * sizeof(float)));
  g.enableVertexAttribArray(2);
  g.vertexAttribPointer(2, 2, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                        sizeof(SpriteVertex), reinterpret_cast<void*>(7 * sizeof(float)));
  g.enableVertexAttribArray(3);
  g.vertexAttribPointer(3, 2, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                        sizeof(SpriteVertex), reinterpret_cast<void*>(9 * sizeof(float)));
  g.bindVertexArray(0);
}

}  // namespace

SpriteRenderer::SpriteRenderer() = default;

SpriteRenderer::~SpriteRenderer() { shutdown(); }

bool SpriteRenderer::init() {
  if (initialized_) {
    return true;
  }
  program_ = gfx::ShaderProgram::create(kVert, kFrag);
  if (program_.id() == 0) {
    return false;
  }
  engine::gl::GlContext& g = engine::gl::gl();
  g.genVertexArrays(1, &vao_);
  g.genBuffers(1, &vbo_);
  configureSpriteVertexLayout(g, vao_, vbo_);
  initialized_ = true;
  return true;
}

void SpriteRenderer::shutdown() {
  if (!initialized_) {
    return;
  }
  engine::gl::GlContext& g = engine::gl::gl();
  if (vbo_ != 0) {
    g.deleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (vao_ != 0) {
    g.deleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  program_ = gfx::ShaderProgram{};
  initialized_ = false;
}

void SpriteRenderer::draw(const SpriteAtlasLibrary& library,
                          const std::vector<SpriteRenderRequest>& requests, const Mat4& mvp,
                          float eyeX, float eyeY, float eyeZ) {
  if (!initialized_ || requests.empty()) {
    return;
  }

  struct BatchSlice {
    const SpriteAtlas* atlas = nullptr;
    int startVertex = 0;
    int vertexCount = 0;
  };

  std::vector<SpriteVertex> verts;
  verts.reserve(requests.size() * 6);
  std::vector<BatchSlice> slices;
  slices.reserve(requests.size());

  SpriteAnimator animator;
  const SpriteAtlas* currentAtlas = nullptr;
  int batchStart = 0;

  auto flushSlice = [&]() {
    if (currentAtlas != nullptr && batchStart < static_cast<int>(verts.size())) {
      slices.push_back(
          BatchSlice{currentAtlas, batchStart, static_cast<int>(verts.size()) - batchStart});
    }
    batchStart = static_cast<int>(verts.size());
  };

  for (const SpriteRenderRequest& request : requests) {
    const SpriteAtlas* atlas = library.findAtlas(request.atlasId);
    if (atlas == nullptr || atlas->textureId == 0) {
      continue;
    }
    const SpriteClip* clip = atlas->findClip(request.clipName);
    if (clip == nullptr || clip->frames.empty()) {
      continue;
    }

    if (currentAtlas != atlas) {
      flushSlice();
      currentAtlas = atlas;
    }

    animator.setClip(clip);
    animator.setTime(request.draw.animTimeSec);
    const SpriteFrame* frame = animator.currentFrame();
    if (frame == nullptr) {
      continue;
    }
    appendSpriteBillboard(verts, request.draw, *frame, *atlas, eyeX, eyeY, eyeZ);
  }
  flushSlice();

  if (verts.empty()) {
    return;
  }

  engine::gl::GlContext& g = engine::gl::gl();
  g.bindVertexArray(vao_);
  g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, vbo_);
  g.bufferData(engine::gl::GlEnum::kArrayBuffer,
               static_cast<engine::gl::GLsizeiptr>(verts.size() * sizeof(SpriteVertex)),
               verts.data(), engine::gl::GlEnum::kDynamicDraw);

  program_.use();
  program_.setMat4("uMvp", mvp);
  program_.setInt("uTex", 0);

  for (const BatchSlice& slice : slices) {
    if (slice.vertexCount <= 0 || slice.atlas == nullptr) {
      continue;
    }
    g.activeTexture(engine::gl::GlEnum::kTexture0);
    g.bindTexture(engine::gl::GlEnum::kTexture2D, slice.atlas->textureId);
    g.drawArrays(engine::gl::GlEnum::kTriangles, slice.startVertex, slice.vertexCount);
  }

  g.bindTexture(engine::gl::GlEnum::kTexture2D, 0);
  g.bindVertexArray(0);
}

}  // namespace evolab::engine::gfx::sprites
