#include "game/GameRenderer.hpp"

#include "engine/gl/GlContext.hpp"
#include "engine/gfx/sprites/SpriteAtlasLibrary.hpp"
#include "game/GameShaders.hpp"
#include "game/OrganismDrawer.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonInformation.hpp"
#include "sim/EnergonString.hpp"
#include "sim/Organism.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace evolab::game {

namespace {

using engine::gl::GlContext;

struct EnergonVertex {
  float x, y, z;
  float r, g, b, a;
};

void pushEnergonVertex(std::vector<EnergonVertex>& out, float x, float y, float z, float r, float g,
                        float b, float a) {
  out.push_back({x, y, z, r, g, b, a});
}

void appendSunfallGroundPatch(std::vector<EnergonVertex>& verts, float x, float y, float z,
                              float arm, float pillar, float r, float g, float b, float alpha) {
  pushEnergonVertex(verts, x - arm, y, z, r, g, b, alpha);
  pushEnergonVertex(verts, x + arm, y, z, r, g, b, alpha * 0.85f);
  pushEnergonVertex(verts, x, y, z - arm, r, g, b, alpha);
  pushEnergonVertex(verts, x, y, z + arm, r, g, b, alpha * 0.85f);
  pushEnergonVertex(verts, x, y, z, r, g, b, alpha);
  pushEnergonVertex(verts, x, y + pillar, z, r * 1.08f, g * 1.05f, b * 0.95f, alpha * 0.55f);
}

void appendBlobStreak(std::vector<EnergonVertex>& verts, const EnergonBlob& blob) {
  const float sizeScale = static_cast<float>(blob.initialBytes);
  const float byteNorm = sizeScale / 8.0f;
  const bool isSunfall = blob.origin == evolab::EnergonOrigin::Sunfall;

  const std::uint8_t tailByte =
      blob.remaining > 0 ? evolab::energonByteAt(blob, 0)
                         : static_cast<std::uint8_t>(blob.data & 0xFFu);
  const std::uint8_t headByte =
      blob.remaining > 0 ? evolab::energonByteAt(blob, std::max(0, blob.remaining - 1))
                         : tailByte;

  float tr = 0.0f;
  float tg = 0.0f;
  float tb = 0.0f;
  float hr = 0.0f;
  float hg = 0.0f;
  float hb = 0.0f;
  evolab::energonPaletteRgb(tailByte, tr, tg, tb);
  evolab::energonPaletteRgb(headByte, hr, hg, hb);

  float r = (tr + hr) * 0.5f;
  float g = (tg + hg) * 0.5f;
  float b = (tb + hb) * 0.5f;

  float alpha = std::min(1.0f, std::max(0.08f, blob.ttl / 50.0f));
  if (blob.origin == evolab::EnergonOrigin::Fragment) {
    r = std::min(1.0f, r * 0.35f + 0.65f);
    g *= 0.15f;
    b *= 0.10f;
    alpha = std::min(1.0f, alpha + 0.35f);
  } else if (blob.grounded && !blob.onWet) {
    const float grey = 0.35f + 0.15f * byteNorm;
    r = grey * 0.6f + r * 0.4f;
    g = grey * 0.6f + g * 0.4f;
    b = grey * 0.6f + b * 0.4f;
    alpha *= 0.65f;
  } else if (blob.onWet && blob.origin != evolab::EnergonOrigin::Fragment) {
    r += 0.08f * byteNorm;
    g += 0.06f * byteNorm;
    alpha = std::min(1.0f, alpha + 0.15f);
    if (isSunfall && blob.grounded) {
      alpha = std::max(0.45f, alpha);
    }
  }

  if (!blob.grounded) {
    const float streak = isSunfall ? (2.0f + sizeScale * 3.0f) : (1.4f + sizeScale * 2.6f);
    pushEnergonVertex(verts, blob.x, blob.y, blob.z, r, g, b, alpha);
    pushEnergonVertex(verts, blob.x, blob.y - streak, blob.z, tr * 0.7f, tg * 0.7f, tb * 0.6f,
                      0.0f);
    const float w = 0.08f + byteNorm * 0.12f;
    pushEnergonVertex(verts, blob.x + w, blob.y, blob.z, hr, hg, hb, alpha * 0.85f);
    pushEnergonVertex(verts, blob.x + w, blob.y - streak, blob.z, tr * 0.7f, tg * 0.7f, tb * 0.6f,
                      0.0f);
  } else {
    const float pillar =
        isSunfall ? (1.15f + sizeScale * 0.95f) : (0.35f + sizeScale * 0.55f);
    const bool hasSegment =
        blob.remaining > 1 &&
        (std::abs(blob.headX - blob.tailX) > 1.0e-3f || std::abs(blob.headZ - blob.tailZ) > 1.0e-3f);
    if (isSunfall && blob.onWet && !hasSegment) {
      const float arm = 0.35f + byteNorm * 0.55f;
      appendSunfallGroundPatch(verts, blob.x, blob.y, blob.z, arm, pillar, r, g, b, alpha);
    } else if (isSunfall && blob.onWet && hasSegment) {
      pushEnergonVertex(verts, blob.tailX, blob.y, blob.tailZ, tr, tg, tb, alpha);
      pushEnergonVertex(verts, blob.headX, blob.y, blob.headZ, hr, hg, hb, alpha * 0.9f);
      const float arm = 0.28f + byteNorm * 0.4f;
      appendSunfallGroundPatch(verts, blob.x, blob.y, blob.z, arm, pillar * 0.65f, r, g, b,
                               alpha * 0.85f);
    } else if (hasSegment) {
      pushEnergonVertex(verts, blob.tailX, blob.y, blob.tailZ, tr, tg, tb, alpha);
      pushEnergonVertex(verts, blob.headX, blob.y, blob.headZ, hr, hg, hb, alpha * 0.85f);
      pushEnergonVertex(verts, blob.x, blob.y, blob.z, r, g, b, alpha * 0.9f);
      pushEnergonVertex(verts, blob.x, blob.y + pillar * 0.5f, blob.z, r, g, b, alpha * 0.25f);
    } else {
      pushEnergonVertex(verts, blob.x, blob.y, blob.z, r, g, b, alpha);
      pushEnergonVertex(verts, blob.x, blob.y + pillar, blob.z, hr, hg, hb, alpha * 0.35f);
      const float w = 0.05f + byteNorm * 0.1f;
      pushEnergonVertex(verts, blob.x + w, blob.y, blob.z, r, g, b, alpha * 0.9f);
      pushEnergonVertex(verts, blob.x + w, blob.y + pillar * 0.7f, blob.z, r, g, b, alpha * 0.2f);
    }
  }
}

}  // namespace

GameRenderer::GameRenderer() = default;

GameRenderer::~GameRenderer() { shutdown(); }

bool GameRenderer::init(const std::function<void()>& heartbeat, const std::string& exeBasePath) {
  if (initialized_) {
    return true;
  }

  const auto beat = [&heartbeat]() {
    if (heartbeat) {
      heartbeat();
    }
  };

  terrainProgram_ = engine::gfx::ShaderProgram::create(kTerrainVert, kTerrainFrag);
  beat();
  waterProgram_ = engine::gfx::ShaderProgram::create(kWaterVert, kWaterFrag);
  beat();
  energonProgram_ = engine::gfx::ShaderProgram::create(kEnergonVert, kEnergonFrag);
  beat();
  cellProgram_ = engine::gfx::ShaderProgram::create(kCellVert, kCellFrag);
  beat();

  if (!spriteRenderer_.init()) {
    std::cerr << "WARNING: sprite renderer init failed\n";
  }

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

  if (!exeBasePath.empty()) {
    const bool mouthLoaded = spriteLibrary_.loadAtlas(
        "mouth", engine::gfx::sprites::kDefaultMouthSpriteManifestRelPath, exeBasePath);
    const bool perceptorLoaded = spriteLibrary_.loadAtlas(
        "perceptor", engine::gfx::sprites::kDefaultPerceptorSpriteManifestRelPath, exeBasePath);
    const bool actuatorLoaded = spriteLibrary_.loadAtlas(
        "actuator", engine::gfx::sprites::kDefaultActuatorSpriteManifestRelPath, exeBasePath);
    spritesLoaded_ = mouthLoaded || perceptorLoaded || actuatorLoaded;
    if (spritesLoaded_) {
      spriteLibrary_.uploadAll();
    }
    if (!mouthLoaded) {
      std::cerr << "WARNING: mouth sprite atlas failed to load — mouths render as solid circles\n";
    }
    if (!perceptorLoaded) {
      std::cerr << "WARNING: perceptor sprite atlas failed to load\n";
    }
    if (!actuatorLoaded) {
      std::cerr << "WARNING: actuator sprite atlas failed to load\n";
    }
  }

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
  spriteRenderer_.shutdown();
  spriteLibrary_.shutdown();
  spritesLoaded_ = false;

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
  verts.reserve(std::min(blobs.size(), static_cast<std::size_t>(8192)) * 4);
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
                                 const engine::OrbitCamera& camera, int viewportW, int viewportH,
                                 std::uint64_t simTick, float fixedSimHz) {
  if (!initialized_ || organisms.empty()) {
    return;
  }

  float eyeX = 0.0f;
  float eyeY = 0.0f;
  float eyeZ = 0.0f;
  camera.eyePosition(eyeX, eyeY, eyeZ);

  const engine::Mat4 mvp = viewProjMatrix(camera, viewportW, viewportH);
  const OrganismDrawBatch batch = buildOrganismDrawBatch(
      organisms, eyeX, eyeY, eyeZ, mvp, viewportW, viewportH, simTick, fixedSimHz, kWorldCellSize,
      showNeuronDiagnostics_);

  engine::gl::GlContext& g = engine::gl::gl();
  g.viewport(0, 0, viewportW, viewportH);

  auto drawLineVerts = [&](const std::vector<OrganismLineVertex>& verts) {
    if (verts.empty()) {
      return;
    }
    energonVertexCount_ = static_cast<int>(verts.size());
    g.bindVertexArray(energonVao_);
    g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, energonVbo_);
    g.bufferData(engine::gl::GlEnum::kArrayBuffer,
                 static_cast<engine::gl::GLsizeiptr>(verts.size() * sizeof(OrganismLineVertex)),
                 verts.data(), engine::gl::GlEnum::kDynamicDraw);
    g.enableVertexAttribArray(0);
    g.vertexAttribPointer(0, 3, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                          sizeof(OrganismLineVertex), reinterpret_cast<void*>(0));
    g.enableVertexAttribArray(1);
    g.vertexAttribPointer(1, 4, engine::gl::GlEnum::kFloat, engine::gl::GlEnum::kFalse,
                          sizeof(OrganismLineVertex), reinterpret_cast<void*>(3 * sizeof(float)));
    energonProgram_.use();
    energonProgram_.setMat4("uMvp", mvp);
    g.drawArrays(engine::gl::GlEnum::kLines, 0, energonVertexCount_);
  };

  g.enable(engine::gl::GlEnum::kDepthTest);
  g.enable(engine::gl::GlEnum::kBlend);
  g.blendFunc(engine::gl::GlEnum::kSrcAlpha, engine::gl::GlEnum::kOneMinusSrcAlpha);
  drawLineVerts(batch.boneLineVerts);
  drawLineVerts(batch.neuralLineVerts);

  g.disable(engine::gl::GlEnum::kDepthTest);

  if (!batch.cellVerts.empty()) {
    cellVertexCount_ = static_cast<int>(batch.cellVerts.size());
    g.bindVertexArray(cellVao_);
    g.bindBuffer(engine::gl::GlEnum::kArrayBuffer, cellVbo_);
    g.bufferData(engine::gl::GlEnum::kArrayBuffer,
                 static_cast<engine::gl::GLsizeiptr>(batch.cellVerts.size() * sizeof(CellVertex)),
                 batch.cellVerts.data(), engine::gl::GlEnum::kDynamicDraw);
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

  if (spritesLoaded_ && !batch.spriteInstances.empty()) {
    std::vector<engine::gfx::sprites::SpriteRenderRequest> spriteRequests;
    spriteRequests.reserve(batch.spriteInstances.size());
    for (const OrganismSpriteInstance& instance : batch.spriteInstances) {
      engine::gfx::sprites::SpriteRenderRequest req;
      req.atlasId = instance.atlasId != nullptr ? instance.atlasId : "";
      req.clipName = instance.clipName != nullptr ? instance.clipName : "";
      req.draw = instance.draw;
      spriteRequests.push_back(std::move(req));
    }
    spriteRenderer_.draw(spriteLibrary_, spriteRequests, mvp, eyeX, eyeY, eyeZ);
  }

  g.enable(engine::gl::GlEnum::kDepthTest);
  g.bindVertexArray(0);
}

}  // namespace evolab::game
