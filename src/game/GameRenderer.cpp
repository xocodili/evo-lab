#include "game/GameRenderer.hpp"

#include "engine/gl/GlContext.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Energon.hpp"
#include "sim/Organism.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace evolab::game {

namespace {

using engine::gl::GlContext;

const char* kTerrainVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMvp;
out vec3 vColor;
void main() {
  vColor = aColor;
  gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kTerrainFrag = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
  FragColor = vec4(vColor, 1.0);
}
)";

const char* kWaterVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMvp;
void main() {
  gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kWaterFrag = R"(#version 330 core
out vec4 FragColor;
void main() {
  FragColor = vec4(0.08, 0.35, 0.55, 0.45);
}
)";

const char* kEnergonVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uMvp;
out vec4 vColor;
void main() {
  vColor = aColor;
  gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kEnergonFrag = R"(#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
  FragColor = vColor;
}
)";

const char* kCellVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aLocal;
uniform mat4 uMvp;
out vec4 vColor;
out vec2 vLocal;
void main() {
  vColor = aColor;
  vLocal = aLocal;
  gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kCellFrag = R"(#version 330 core
in vec4 vColor;
in vec2 vLocal;
out vec4 FragColor;
void main() {
  if (dot(vLocal, vLocal) > 1.0) {
    discard;
  }
  FragColor = vColor;
}
)";

struct CellVertex {
  float x, y, z;
  float r, g, b, a;
  float lx, ly;
};

void appendCellBillboard(std::vector<CellVertex>& verts, float wx, float wy, float wz, float eyeX,
                         float eyeY, float eyeZ, float r, float g, float b, float a,
                         float halfSize) {
  float toEyeX = eyeX - wx;
  float toEyeY = eyeY - wy;
  float toEyeZ = eyeZ - wz;
  float toEyeLen = std::sqrt(toEyeX * toEyeX + toEyeY * toEyeY + toEyeZ * toEyeZ);
  if (toEyeLen <= 0.0001f) {
    return;
  }
  toEyeX /= toEyeLen;
  toEyeY /= toEyeLen;
  toEyeZ /= toEyeLen;

  float rightX = toEyeZ;
  float rightY = 0.0f;
  float rightZ = -toEyeX;
  float rightLen = std::sqrt(rightX * rightX + rightZ * rightZ);
  if (rightLen <= 0.0001f) {
    rightX = 1.0f;
    rightZ = 0.0f;
    rightLen = 1.0f;
  }
  rightX /= rightLen;
  rightZ /= rightLen;

  const float upX = rightY * toEyeZ - rightZ * toEyeY;
  const float upY = rightZ * toEyeX - rightX * toEyeZ;
  const float upZ = rightX * toEyeY - rightY * toEyeX;

  const struct {
    float lx, ly, sx, sy;
  } corners[] = {
      {-1.0f, -1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, 1.0f, -1.0f},
      {1.0f, 1.0f, 1.0f, 1.0f},     {-1.0f, 1.0f, -1.0f, 1.0f},
  };

  CellVertex quad[4];
  for (int i = 0; i < 4; ++i) {
    const auto& corner = corners[i];
    const float px = wx + halfSize * (corner.sx * rightX + corner.sy * upX);
    const float py = wy + halfSize * (corner.sx * rightY + corner.sy * upY);
    const float pz = wz + halfSize * (corner.sx * rightZ + corner.sy * upZ);
    quad[i] = {px, py, pz, r, g, b, a, corner.lx, corner.ly};
  }

  verts.push_back(quad[0]);
  verts.push_back(quad[1]);
  verts.push_back(quad[2]);
  verts.push_back(quad[0]);
  verts.push_back(quad[2]);
  verts.push_back(quad[3]);
}

bool linkWorldDistanceOk(const SkeletonNode& a, const SkeletonNode& b, float maxLength) {
  const float dx = b.worldX - a.worldX;
  const float dz = b.worldZ - a.worldZ;
  const float distSq = dx * dx + dz * dz;
  if (distSq < 1.0e-4f) {
    return false;
  }
  const float maxSpan = std::max(maxLength * 2.5f, 0.5f);
  return distSq <= maxSpan * maxSpan;
}

struct EnergonVertex {
  float x, y, z;
  float r, g, b, a;
};

void pushEnergonVertex(std::vector<EnergonVertex>& out, float x, float y, float z, float r, float g,
                        float b, float a) {
  out.push_back({x, y, z, r, g, b, a});
}

void appendLinkLine(std::vector<EnergonVertex>& verts, const SkeletonNode& a, const SkeletonNode& b,
                    float yOffset, float r, float g, float colB, float alpha, float maxLength) {
  if (!linkWorldDistanceOk(a, b, maxLength)) {
    return;
  }
  const float y = std::max(a.worldY, b.worldY) + yOffset;
  pushEnergonVertex(verts, a.worldX, y, a.worldZ, r, g, colB, alpha);
  pushEnergonVertex(verts, b.worldX, y, b.worldZ, r * 0.95f, g * 0.95f, colB * 0.95f, alpha * 0.9f);
}

void appendGroundBoneStrip(std::vector<CellVertex>& verts, const SkeletonNode& parent,
                           const SkeletonNode& child, float r, float g, float b, float a,
                           float halfWidth, float maxLength) {
  if (!linkWorldDistanceOk(parent, child, maxLength)) {
    return;
  }
  const float dx = child.worldX - parent.worldX;
  const float dz = child.worldZ - parent.worldZ;
  const float lenSq = dx * dx + dz * dz;
  if (lenSq < 1.0e-6f) {
    return;
  }
  const float len = std::sqrt(lenSq);
  const float nx = -dz / len;
  const float nz = dx / len;
  const float y = std::max(parent.worldY, child.worldY) + 0.28f;

  const float px0 = parent.worldX - nx * halfWidth;
  const float pz0 = parent.worldZ - nz * halfWidth;
  const float px1 = parent.worldX + nx * halfWidth;
  const float pz1 = parent.worldZ + nz * halfWidth;
  const float cx0 = child.worldX - nx * halfWidth;
  const float cz0 = child.worldZ - nz * halfWidth;
  const float cx1 = child.worldX + nx * halfWidth;
  const float cz1 = child.worldZ + nz * halfWidth;

  const CellVertex quad[] = {
      {px0, y, pz0, r, g, b, a, 0.0f, 0.0f}, {px1, y, pz1, r, g, b, a, 0.0f, 0.0f},
      {cx1, y, cz1, r, g, b, a * 0.85f, 0.0f, 0.0f}, {px0, y, pz0, r, g, b, a, 0.0f, 0.0f},
      {cx1, y, cz1, r, g, b, a * 0.85f, 0.0f, 0.0f}, {cx0, y, cz0, r, g, b, a * 0.85f, 0.0f, 0.0f},
  };
  for (const CellVertex& vertex : quad) {
    verts.push_back(vertex);
  }
}

void appendHeadingArrow(std::vector<EnergonVertex>& verts, float wx, float wy, float wz, float heading,
                        float length) {
  const float y = wy + 0.35f;
  const float fx = std::sin(heading);
  const float fz = std::cos(heading);
  const float hub = length * 0.15f;
  const float tip = length * 1.05f;
  const float wing = length * 0.22f;
  const float px0 = wx + fx * hub;
  const float pz0 = wz + fz * hub;
  const float px1 = wx + fx * tip;
  const float pz1 = wz + fz * tip;
  const float lx = wx + fx * (tip - wing) - fz * wing * 0.45f;
  const float lz = wz + fz * (tip - wing) + fx * wing * 0.45f;
  const float rx = wx + fx * (tip - wing) + fz * wing * 0.45f;
  const float rz = wz + fz * (tip - wing) - fx * wing * 0.45f;

  pushEnergonVertex(verts, px0, y, pz0, 0.2f, 1.0f, 1.0f, 1.0f);
  pushEnergonVertex(verts, px1, y, pz1, 0.2f, 1.0f, 1.0f, 1.0f);
  pushEnergonVertex(verts, lx, y, lz, 0.2f, 1.0f, 1.0f, 0.85f);
  pushEnergonVertex(verts, px1, y, pz1, 0.2f, 1.0f, 1.0f, 1.0f);
  pushEnergonVertex(verts, rx, y, rz, 0.2f, 1.0f, 1.0f, 0.85f);
}

void appendBlobStreak(std::vector<EnergonVertex>& verts, const EnergonBlob& blob) {
  const float sizeScale = static_cast<float>(blob.initialBytes);
  const float byteNorm = sizeScale / 8.0f;

  const std::uint8_t br = static_cast<std::uint8_t>((blob.data >> 0) & 0xFF);
  const std::uint8_t bg = static_cast<std::uint8_t>((blob.data >> 8) & 0xFF);
  const std::uint8_t bb = static_cast<std::uint8_t>((blob.data >> 16) & 0xFF);

  float r = 0.55f + 0.4f * (static_cast<float>(br) / 255.0f);
  float g = 0.50f + 0.45f * (static_cast<float>(bg) / 255.0f);
  float b = 0.20f + 0.35f * (static_cast<float>(bb) / 255.0f);

  float alpha = std::min(1.0f, std::max(0.08f, blob.ttl / 50.0f));
  if (blob.origin == evolab::EnergonOrigin::Fragment) {
    // M-cloaca spit — siren red (rejected / overflow byte).
    r = 1.0f;
    g = 0.10f;
    b = 0.06f;
    alpha = std::min(1.0f, alpha + 0.35f);
  } else if (blob.grounded && !blob.onWet) {
    const float grey = 0.35f + 0.15f * byteNorm;
    r = grey;
    g = grey * 0.95f;
    b = grey * 0.85f;
    alpha *= 0.65f;
  } else if (blob.onWet && blob.origin != evolab::EnergonOrigin::Fragment) {
    r += 0.12f * byteNorm;
    g += 0.10f * byteNorm;
    alpha = std::min(1.0f, alpha + 0.15f);
  }

  if (!blob.grounded) {
    const float streak = 1.4f + sizeScale * 2.6f;
    pushEnergonVertex(verts, blob.x, blob.y, blob.z, r, g, b, alpha);
    pushEnergonVertex(verts, blob.x, blob.y - streak, blob.z, r * 0.7f, g * 0.7f, b * 0.6f, 0.0f);
    const float w = 0.08f + byteNorm * 0.12f;
    pushEnergonVertex(verts, blob.x + w, blob.y, blob.z, r, g, b, alpha * 0.85f);
    pushEnergonVertex(verts, blob.x + w, blob.y - streak, blob.z, r * 0.7f, g * 0.7f, b * 0.6f, 0.0f);
  } else {
    const float pillar = 0.35f + sizeScale * 0.55f;
    const bool hasSegment =
        blob.remaining > 1 &&
        (std::abs(blob.headX - blob.tailX) > 1.0e-3f || std::abs(blob.headZ - blob.tailZ) > 1.0e-3f);
    if (hasSegment) {
      pushEnergonVertex(verts, blob.tailX, blob.y, blob.tailZ, r, g, b, alpha);
      pushEnergonVertex(verts, blob.headX, blob.y, blob.headZ, r * 1.05f, g * 1.02f, b * 0.95f,
                        alpha * 0.85f);
      pushEnergonVertex(verts, blob.x, blob.y, blob.z, r, g, b, alpha * 0.9f);
      pushEnergonVertex(verts, blob.x, blob.y + pillar * 0.5f, blob.z, r, g, b, alpha * 0.25f);
    } else {
      pushEnergonVertex(verts, blob.x, blob.y, blob.z, r, g, b, alpha);
      pushEnergonVertex(verts, blob.x, blob.y + pillar, blob.z, r * 1.1f, g * 1.05f, b * 0.9f,
                        alpha * 0.35f);
      const float w = 0.05f + byteNorm * 0.1f;
      pushEnergonVertex(verts, blob.x + w, blob.y, blob.z, r, g, b, alpha * 0.9f);
      pushEnergonVertex(verts, blob.x + w, blob.y + pillar * 0.7f, blob.z, r, g, b, alpha * 0.2f);
    }
  }
}

}  // namespace

GameRenderer::GameRenderer() = default;

GameRenderer::~GameRenderer() { shutdown(); }

bool GameRenderer::init() {
  if (initialized_) {
    return true;
  }

  terrainProgram_ = engine::gfx::ShaderProgram::create(kTerrainVert, kTerrainFrag);
  waterProgram_ = engine::gfx::ShaderProgram::create(kWaterVert, kWaterFrag);
  energonProgram_ = engine::gfx::ShaderProgram::create(kEnergonVert, kEnergonFrag);
  cellProgram_ = engine::gfx::ShaderProgram::create(kCellVert, kCellFrag);

  engine::gl::GlContext& g = engine::gl::gl();
  g.genVertexArrays(1, &terrainVao_);
  g.genBuffers(1, &terrainVbo_);
  g.genBuffers(1, &terrainEbo_);
  g.genVertexArrays(1, &waterVao_);
  g.genBuffers(1, &waterVbo_);
  g.genVertexArrays(1, &energonVao_);
  g.genBuffers(1, &energonVbo_);
  g.genVertexArrays(1, &cellVao_);
  g.genBuffers(1, &cellVbo_);

  initialized_ = true;
  return true;
}

void GameRenderer::shutdown() {
  if (!initialized_) {
    return;
  }

  engine::gl::GlContext& g = engine::gl::gl();
  if (terrainEbo_) {
    g.deleteBuffers(1, &terrainEbo_);
    terrainEbo_ = 0;
  }
  if (terrainVbo_) {
    g.deleteBuffers(1, &terrainVbo_);
    terrainVbo_ = 0;
  }
  if (terrainVao_) {
    g.deleteVertexArrays(1, &terrainVao_);
    terrainVao_ = 0;
  }
  if (waterVbo_) {
    g.deleteBuffers(1, &waterVbo_);
    waterVbo_ = 0;
  }
  if (waterVao_) {
    g.deleteVertexArrays(1, &waterVao_);
    waterVao_ = 0;
  }
  if (energonVbo_) {
    g.deleteBuffers(1, &energonVbo_);
    energonVbo_ = 0;
  }
  if (energonVao_) {
    g.deleteVertexArrays(1, &energonVao_);
    energonVao_ = 0;
  }
  if (cellVbo_) {
    g.deleteBuffers(1, &cellVbo_);
    cellVbo_ = 0;
  }
  if (cellVao_) {
    g.deleteVertexArrays(1, &cellVao_);
    cellVao_ = 0;
  }

  terrainProgram_ = engine::gfx::ShaderProgram{};
  waterProgram_ = engine::gfx::ShaderProgram{};
  energonProgram_ = engine::gfx::ShaderProgram{};
  cellProgram_ = engine::gfx::ShaderProgram{};
  terrainGeometryUploaded_ = false;
  initialized_ = false;
}

void GameRenderer::beginFrame(float clearR, float clearG, float clearB) {
  engine::gl::GlContext& g = engine::gl::gl();
  g.clearColor(clearR, clearG, clearB, 1.0f);
  g.clear(engine::gl::GlEnum::kColorBufferBit | engine::gl::GlEnum::kDepthBufferBit);
  g.enable(engine::gl::GlEnum::kDepthTest);
  g.enable(engine::gl::GlEnum::kBlend);
  g.blendFunc(engine::gl::GlEnum::kSrcAlpha, engine::gl::GlEnum::kOneMinusSrcAlpha);
}

engine::Mat4 GameRenderer::viewProjMatrix(const engine::OrbitCamera& camera, int viewportW,
                                          int viewportH) const {
  const float aspect =
      viewportW > 0 ? static_cast<float>(viewportW) / static_cast<float>(viewportH) : 1.0f;
  const engine::Mat4 proj =
      engine::mat4Perspective(60.0f * 3.1415926535f / 180.0f, aspect, 0.1f, 800.0f);
  return engine::mat4Multiply(proj, camera.viewMatrix());
}

void GameRenderer::uploadTerrainGeometry(const TerrainMesh& mesh) {
  if (!initialized_) {
    return;
  }

  engine::gl::GlContext& g = engine::gl::gl();
  indexCount_ = static_cast<int>(mesh.indices.size());

  g.bindVertexArray(terrainVao_);
  g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, terrainVbo_);
  g.bufferData(engine::gl::GlEnum::kArrayBuffer,
               static_cast<engine::gl::GLsizeiptr>(mesh.vertices.size() * sizeof(TerrainVertex)),
               mesh.vertices.data(), engine::gl::GlEnum::kDynamicDraw);

  g.bindBuffer(engine::gl::GlEnum::kElementArrayBuffer, terrainEbo_);
  g.bufferData(engine::gl::GlEnum::kElementArrayBuffer,
               static_cast<engine::gl::GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
               mesh.indices.data(), engine::gl::GlEnum::kStaticDraw);

  g.enableVertexAttribArray(0);
  g.vertexAttribPointer(0, 3, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse, sizeof(TerrainVertex),
                        reinterpret_cast<void*>(0));
  g.enableVertexAttribArray(1);
  g.vertexAttribPointer(1, 3, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse, sizeof(TerrainVertex),
                        reinterpret_cast<void*>(3 * sizeof(float)));
  g.bindVertexArray(0);

  terrainGeometryUploaded_ = indexCount_ > 0;
}

void GameRenderer::uploadTerrainColors(const TerrainMesh& mesh) {
  if (!initialized_ || !terrainGeometryUploaded_) {
    return;
  }

  engine::gl::GlContext& g = engine::gl::gl();
  g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, terrainVbo_);
  g.bufferData(engine::gl::GlEnum::kArrayBuffer,
               static_cast<engine::gl::GLsizeiptr>(mesh.vertices.size() * sizeof(TerrainVertex)),
               mesh.vertices.data(), engine::gl::GlEnum::kDynamicDraw);
  g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, 0);
}

void GameRenderer::drawTerrain(const engine::OrbitCamera& camera, int viewportW, int viewportH) {
  if (!initialized_ || !terrainGeometryUploaded_) {
    return;
  }

  engine::gl::GlContext& g = engine::gl::gl();
  g.viewport(0, 0, viewportW, viewportH);
  const engine::Mat4 mvp = viewProjMatrix(camera, viewportW, viewportH);

  terrainProgram_.use();
  terrainProgram_.setMat4("uMvp", mvp);

  g.bindVertexArray(terrainVao_);
  g.drawElements(engine::gl::GlEnum::kTriangles, indexCount_, engine::gl::GlEnum::kUnsignedInt, nullptr);
  g.bindVertexArray(0);
}

void GameRenderer::drawWaterPlane(const TerrainMesh& mesh, float waterLevelScaled,
                                  const engine::OrbitCamera& camera, int viewportW, int viewportH) {
  if (!initialized_) {
    return;
  }

  engine::gl::GlContext& g = engine::gl::gl();
  const engine::Mat4 mvp = viewProjMatrix(camera, viewportW, viewportH);

  const float waterVerts[] = {
      mesh.minX, waterLevelScaled, mesh.minZ, mesh.maxX, waterLevelScaled, mesh.minZ,
      mesh.maxX, waterLevelScaled, mesh.maxZ, mesh.minX, waterLevelScaled, mesh.maxZ,
  };

  g.bindVertexArray(waterVao_);
  g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, waterVbo_);
  g.bufferData(engine::gl::GlEnum::kArrayBuffer, sizeof(waterVerts), waterVerts, engine::gl::GlEnum::kDynamicDraw);
  g.enableVertexAttribArray(0);
  g.vertexAttribPointer(0, 3, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse, 3 * sizeof(float),
                        reinterpret_cast<void*>(0));

  waterProgram_.use();
  waterProgram_.setMat4("uMvp", mvp);
  g.drawArrays(engine::gl::GlEnum::kTriangleFan, 0, 4);
  g.bindVertexArray(0);
}

void GameRenderer::drawEnergon(const std::vector<EnergonBlob>& blobs, const engine::OrbitCamera& camera,
                               int viewportW, int viewportH) {
  if (!initialized_ || blobs.empty()) {
    return;
  }

  std::vector<EnergonVertex> verts;
  verts.reserve(blobs.size() * 4);
  for (const EnergonBlob& blob : blobs) {
    appendBlobStreak(verts, blob);
  }
  if (verts.empty()) {
    return;
  }

  engine::gl::GlContext& g = engine::gl::gl();
  energonVertexCount_ = static_cast<int>(verts.size());

  g.viewport(0, 0, viewportW, viewportH);
  const engine::Mat4 mvp = viewProjMatrix(camera, viewportW, viewportH);

  g.bindVertexArray(energonVao_);
  g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, energonVbo_);
  g.bufferData(engine::gl::GlEnum::kArrayBuffer,
               static_cast<engine::gl::GLsizeiptr>(verts.size() * sizeof(EnergonVertex)), verts.data(),
               engine::gl::GlEnum::kDynamicDraw);
  g.enableVertexAttribArray(0);
  g.vertexAttribPointer(0, 3, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse, sizeof(EnergonVertex),
                        reinterpret_cast<void*>(0));
  g.enableVertexAttribArray(1);
  g.vertexAttribPointer(1, 4, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse, sizeof(EnergonVertex),
                        reinterpret_cast<void*>(3 * sizeof(float)));

  energonProgram_.use();
  energonProgram_.setMat4("uMvp", mvp);
  g.drawArrays(engine::gl::GlEnum::kLines, 0, energonVertexCount_);
  g.bindVertexArray(0);
}

void GameRenderer::drawOrganisms(const std::vector<Organism>& organisms,
                                 const engine::OrbitCamera& camera, int viewportW, int viewportH) {
  if (!initialized_ || organisms.empty()) {
    return;
  }

  float eyeX = 0.0f;
  float eyeY = 0.0f;
  float eyeZ = 0.0f;
  camera.eyePosition(eyeX, eyeY, eyeZ);

  std::vector<CellVertex> nodeVerts;
  std::vector<EnergonVertex> boneLineVerts;
  std::vector<EnergonVertex> neuralLineVerts;
  std::vector<EnergonVertex> headingVerts;
  nodeVerts.reserve(organisms.size() * 24);
  boneLineVerts.reserve(organisms.size() * 16);
  neuralLineVerts.reserve(organisms.size() * 8);
  headingVerts.reserve(organisms.size() * 8);

  for (const Organism& organism : organisms) {
    if (!organism.alive) {
      continue;
    }

    const float fill = static_cast<float>(organism.bodyStorage.size()) /
                       static_cast<float>(kStemCellStorageMaxBytes);
    const float alpha = 0.55f + 0.45f * std::min(1.0f, fill);
    float maxBoneLen = 0.0f;
    for (const SkeletonLink& link : organism.links) {
      maxBoneLen = std::max(maxBoneLen, link.restLength);
    }

    for (const SkeletonLink& link : organism.links) {
      const SkeletonNode* parent = organism.findNode(link.parentNodeId);
      const SkeletonNode* child = organism.findNode(link.childNodeId);
      if (parent == nullptr || child == nullptr) {
        continue;
      }
      appendGroundBoneStrip(nodeVerts, *parent, *child, 0.98f, 0.88f, 0.22f, alpha * 0.95f, 0.16f,
                            link.restLength);
      appendLinkLine(boneLineVerts, *parent, *child, 0.32f, 1.0f, 0.92f, 0.15f, alpha, link.restLength);
    }

    if (!organism.neuralAxons.empty()) {
      const NeuralAxon& axon = organism.neuralAxons.front();
      const SkeletonNode* src = organism.findNode(axon.srcNodeId);
      const SkeletonNode* dst = organism.findNode(axon.dstNodeId);
      if (src != nullptr && dst != nullptr) {
        appendLinkLine(neuralLineVerts, *src, *dst, 0.42f, 0.75f, 0.25f, 1.0f, alpha * 0.95f,
                       maxBoneLen > 0.0f ? maxBoneLen : 1.2f);
      }
    }

    if (const SkeletonNode* root = organism.findNode(organism.rootNodeId)) {
      if (root->neuron == NeuronType::Computer && organism.mouthCount() > 0) {
        const float arrowLen = maxBoneLen > 0.0f ? maxBoneLen : 0.9f;
        appendHeadingArrow(headingVerts, root->worldX, root->worldY, root->worldZ, organism.heading,
                           arrowLen);
      }
    }

    for (const SkeletonNode& node : organism.nodes) {
      float r = 0.72f;
      float g = 0.95f;
      float b = 1.0f;
      float halfSize = 0.16f;
      if (node.neuron == NeuronType::Mouth) {
        r = 1.0f;
        g = 0.62f;
        b = 0.28f;
        halfSize = 0.13f;
      } else if (node.neuron == NeuronType::Computer) {
        r = 0.98f;
        g = 0.92f;
        b = 0.45f;
        halfSize = 0.17f;
      }
      appendCellBillboard(nodeVerts, node.worldX, node.worldY, node.worldZ, eyeX, eyeY, eyeZ, r, g,
                          b, alpha, halfSize);
    }
  }

  engine::gl::GlContext& g = engine::gl::gl();
  g.viewport(0, 0, viewportW, viewportH);
  const engine::Mat4 mvp = viewProjMatrix(camera, viewportW, viewportH);

  auto drawEnergonVerts = [&](const std::vector<EnergonVertex>& verts) {
    if (verts.empty()) {
      return;
    }
    energonVertexCount_ = static_cast<int>(verts.size());
    g.bindVertexArray(energonVao_);
    g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, energonVbo_);
    g.bufferData(engine::gl::GlEnum::kArrayBuffer,
                 static_cast<engine::gl::GLsizeiptr>(verts.size() * sizeof(EnergonVertex)),
                 verts.data(), engine::gl::GlEnum::kDynamicDraw);
    g.enableVertexAttribArray(0);
    g.vertexAttribPointer(0, 3, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                          sizeof(EnergonVertex), reinterpret_cast<void*>(0));
    g.enableVertexAttribArray(1);
    g.vertexAttribPointer(1, 4, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                          sizeof(EnergonVertex), reinterpret_cast<void*>(3 * sizeof(float)));
    energonProgram_.use();
    energonProgram_.setMat4("uMvp", mvp);
    g.drawArrays(engine::gl::GlEnum::kLines, 0, energonVertexCount_);
  };

  g.enable(engine::gl::GlEnum::kDepthTest);
  g.enable(engine::gl::GlEnum::kBlend);
  g.blendFunc(engine::gl::GlEnum::kSrcAlpha, engine::gl::GlEnum::kOneMinusSrcAlpha);
  drawEnergonVerts(boneLineVerts);
  drawEnergonVerts(neuralLineVerts);

  g.disable(engine::gl::GlEnum::kDepthTest);

  if (!nodeVerts.empty()) {
    cellVertexCount_ = static_cast<int>(nodeVerts.size());
    g.bindVertexArray(cellVao_);
    g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, cellVbo_);
    g.bufferData(engine::gl::GlEnum::kArrayBuffer,
                 static_cast<engine::gl::GLsizeiptr>(nodeVerts.size() * sizeof(CellVertex)),
                 nodeVerts.data(), engine::gl::GlEnum::kDynamicDraw);
    g.enableVertexAttribArray(0);
    g.vertexAttribPointer(0, 3, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                          sizeof(CellVertex), reinterpret_cast<void*>(0));
    g.enableVertexAttribArray(1);
    g.vertexAttribPointer(1, 4, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                          sizeof(CellVertex), reinterpret_cast<void*>(3 * sizeof(float)));
    g.enableVertexAttribArray(2);
    g.vertexAttribPointer(2, 2, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                          sizeof(CellVertex), reinterpret_cast<void*>(7 * sizeof(float)));
    cellProgram_.use();
    cellProgram_.setMat4("uMvp", mvp);
    g.drawArrays(engine::gl::GlEnum::kTriangles, 0, cellVertexCount_);
  }

  drawEnergonVerts(headingVerts);

  g.enable(engine::gl::GlEnum::kDepthTest);
  g.bindVertexArray(0);
}

}  // namespace evolab::game
