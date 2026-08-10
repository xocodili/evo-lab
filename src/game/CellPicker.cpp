#include "game/CellPicker.hpp"

#include <algorithm>
#include <cmath>

namespace evolab::game {

namespace {

engine::Mat4 viewProjectionMatrix(const engine::OrbitCamera& camera, int viewportW, int viewportH) {
  const float aspect =
      viewportW > 0 ? static_cast<float>(viewportW) / static_cast<float>(viewportH) : 1.0f;
  const engine::Mat4 proj =
      engine::mat4Perspective(60.0f * 3.1415926535f / 180.0f, aspect, 0.1f, 800.0f);
  return engine::mat4Multiply(proj, camera.viewMatrix());
}

}  // namespace

bool projectWorldToScreen(const engine::OrbitCamera& camera, int viewportW, int viewportH, float wx,
                          float wy, float wz, float& outX, float& outY) {
  if (viewportW <= 0 || viewportH <= 0) {
    return false;
  }

  const engine::Mat4 mvp = viewProjectionMatrix(camera, viewportW, viewportH);
  const float x = mvp.m[0] * wx + mvp.m[4] * wy + mvp.m[8] * wz + mvp.m[12];
  const float y = mvp.m[1] * wx + mvp.m[5] * wy + mvp.m[9] * wz + mvp.m[13];
  const float z = mvp.m[2] * wx + mvp.m[6] * wy + mvp.m[10] * wz + mvp.m[14];
  const float w = mvp.m[3] * wx + mvp.m[7] * wy + mvp.m[11] * wz + mvp.m[15];
  if (w <= 0.0001f) {
    return false;
  }

  const float ndcX = x / w;
  const float ndcY = y / w;
  if (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f) {
    return false;
  }

  outX = (ndcX * 0.5f + 0.5f) * static_cast<float>(viewportW);
  outY = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(viewportH);
  return true;
}

std::uint32_t pickOrganismAtScreen(const std::vector<Organism>& organisms,
                                   const engine::OrbitCamera& camera, int viewportW, int viewportH,
                                   int mouseX, int mouseY, float pickRadiusPx) {
  std::uint32_t bestId = 0;
  float bestDistSq = pickRadiusPx * pickRadiusPx;

  for (const Organism& organism : organisms) {
    if (!organism.alive) {
      continue;
    }
    for (const SkeletonNode& node : organism.nodes) {
      float sx = 0.0f;
      float sy = 0.0f;
      if (!projectWorldToScreen(camera, viewportW, viewportH, node.worldX, node.worldY, node.worldZ,
                                sx, sy)) {
        continue;
      }
      const float dx = sx - static_cast<float>(mouseX);
      const float dy = sy - static_cast<float>(mouseY);
      const float distSq = dx * dx + dy * dy;
      if (distSq <= bestDistSq) {
        bestDistSq = distSq;
        bestId = organism.id;
      }
    }
  }

  return bestId;
}

}  // namespace evolab::game
