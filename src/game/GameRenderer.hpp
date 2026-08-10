#pragma once

#include "engine/Camera.hpp"
#include "engine/gfx/ShaderProgram.hpp"
#include "game/TerrainMesh.hpp"

#include "sim/Energon.hpp"
#include "sim/Organism.hpp"

#include <vector>

namespace evolab::game {

class GameRenderer {
public:
  GameRenderer();
  ~GameRenderer();

  GameRenderer(const GameRenderer&) = delete;
  GameRenderer& operator=(const GameRenderer&) = delete;

  bool init();
  void shutdown();

  void beginFrame(float clearR, float clearG, float clearB);
  void uploadTerrainGeometry(const TerrainMesh& mesh);
  void uploadTerrainColors(const TerrainMesh& mesh);
  void drawTerrain(const engine::OrbitCamera& camera, int viewportW, int viewportH);
  void drawWaterPlane(const TerrainMesh& mesh, float waterLevelScaled, const engine::OrbitCamera& camera,
                      int viewportW, int viewportH);
  void drawEnergon(const std::vector<EnergonBlob>& blobs, const engine::OrbitCamera& camera,
                   int viewportW, int viewportH);
  void drawOrganisms(const std::vector<Organism>& organisms, const engine::OrbitCamera& camera,
                     int viewportW, int viewportH);

private:
  engine::Mat4 viewProjMatrix(const engine::OrbitCamera& camera, int viewportW, int viewportH) const;

  bool initialized_ = false;
  bool terrainGeometryUploaded_ = false;

  engine::gfx::ShaderProgram terrainProgram_;
  engine::gfx::ShaderProgram waterProgram_;
  engine::gfx::ShaderProgram energonProgram_;
  engine::gfx::ShaderProgram cellProgram_;
  unsigned terrainVao_ = 0;
  unsigned terrainVbo_ = 0;
  unsigned terrainEbo_ = 0;
  int indexCount_ = 0;
  unsigned waterVao_ = 0;
  unsigned waterVbo_ = 0;
  unsigned energonVao_ = 0;
  unsigned energonVbo_ = 0;
  int energonVertexCount_ = 0;
  unsigned cellVao_ = 0;
  unsigned cellVbo_ = 0;
  int cellVertexCount_ = 0;
};

}  // namespace evolab::game
