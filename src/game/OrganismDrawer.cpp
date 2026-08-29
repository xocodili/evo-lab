#include "game/OrganismDrawer.hpp"

#include "sim/CellConstants.hpp"
#include "sim/CampTopology.hpp"
#include "sim/Chaos.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/Organism.hpp"

#include <algorithm>
#include <cmath>

namespace evolab::game {

namespace {

bool projectWithMvp(const engine::Mat4& mvp, int viewportW, int viewportH, float wx, float wy,
                    float wz, float& outX, float& outY) {
  if (viewportW <= 0 || viewportH <= 0) {
    return false;
  }
  const float x = mvp.m[0] * wx + mvp.m[4] * wy + mvp.m[8] * wz + mvp.m[12];
  const float y = mvp.m[1] * wx + mvp.m[5] * wy + mvp.m[9] * wz + mvp.m[13];
  const float w = mvp.m[3] * wx + mvp.m[7] * wy + mvp.m[11] * wz + mvp.m[15];
  if (w <= 0.0001f) {
    return false;
  }
  const float ndcX = x / w;
  const float ndcY = y / w;
  outX = (ndcX * 0.5f + 0.5f) * static_cast<float>(viewportW);
  outY = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(viewportH);
  return true;
}

float worldUnitsPerPixel(const engine::Mat4& mvp, int viewportW, int viewportH, float wx, float wy,
                         float wz) {
  float s0x = 0.0f;
  float s0y = 0.0f;
  float s1x = 0.0f;
  float s1y = 0.0f;
  if (!projectWithMvp(mvp, viewportW, viewportH, wx, wy, wz, s0x, s0y) ||
      !projectWithMvp(mvp, viewportW, viewportH, wx + 1.0f, wy, wz, s1x, s1y)) {
    return 0.01f;
  }
  const float pxPerWorld = std::hypot(s1x - s0x, s1y - s0y);
  if (pxPerWorld <= 0.0001f) {
    return 0.01f;
  }
  return 1.0f / pxPerWorld;
}

void appendCellBillboard(std::vector<CellVertex>& verts, float wx, float wy, float wz, float eyeX,
                         float eyeY, float eyeZ, float r, float g, float b, float a,
                         float halfSize) {
  float toEyeX = eyeX - wx;
  float toEyeY = eyeY - wy;
  float toEyeZ = eyeZ - wz;
  const float toEyeLen = std::sqrt(toEyeX * toEyeX + toEyeY * toEyeY + toEyeZ * toEyeZ);
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

void appendGroundLinkStrip(std::vector<CellVertex>& verts, float x0, float z0, float x1, float z1,
                           float y, float halfWidth, float r, float g, float b, float a) {
  const float dx = x1 - x0;
  const float dz = z1 - z0;
  const float lenSq = dx * dx + dz * dz;
  if (lenSq < 1.0e-8f) {
    return;
  }
  const float len = std::sqrt(lenSq);
  const float nx = -dz / len;
  const float nz = dx / len;

  const float px0 = x0 - nx * halfWidth;
  const float pz0 = z0 - nz * halfWidth;
  const float px1 = x0 + nx * halfWidth;
  const float pz1 = z0 + nz * halfWidth;
  const float cx0 = x1 - nx * halfWidth;
  const float cz0 = z1 - nz * halfWidth;
  const float cx1 = x1 + nx * halfWidth;
  const float cz1 = z1 + nz * halfWidth;

  const CellVertex quad[] = {
      {px0, y, pz0, r, g, b, a, 0.0f, 0.0f}, {px1, y, pz1, r, g, b, a, 0.0f, 0.0f},
      {cx1, y, cz1, r, g, b, a * 0.9f, 0.0f, 0.0f}, {px0, y, pz0, r, g, b, a, 0.0f, 0.0f},
      {cx1, y, cz1, r, g, b, a * 0.9f, 0.0f, 0.0f}, {cx0, y, cz0, r, g, b, a * 0.9f, 0.0f, 0.0f},
  };
  for (const CellVertex& vertex : quad) {
    verts.push_back(vertex);
  }
}

void appendShortAxonStub(std::vector<CellVertex>& verts, const SkeletonNode& src,
                         const SkeletonNode& dst, float yOffset, float r, float g, float b,
                         float a, float maxWorldLength, float halfWidth) {
  const float dx = dst.worldX - src.worldX;
  const float dz = dst.worldZ - src.worldZ;
  const float distSq = dx * dx + dz * dz;
  if (distSq < 1.0e-8f || maxWorldLength <= 0.0f) {
    return;
  }
  const float dist = std::sqrt(distSq);
  const float useLen = std::min(dist, maxWorldLength);
  const float nx = dx / dist;
  const float nz = dz / dist;
  const float y = std::max(src.worldY, dst.worldY) + yOffset;
  const float endX = src.worldX + nx * useLen;
  const float endZ = src.worldZ + nz * useLen;
  appendGroundLinkStrip(verts, src.worldX, src.worldZ, endX, endZ, y, halfWidth, r, g, b, a);
}

void appendHeadingChevron(std::vector<CellVertex>& verts, float wx, float wy, float wz, float heading,
                          float length, float r, float g, float b, float a) {
  const float y = wy + 0.04f;
  const float fx = std::sin(heading);
  const float fz = std::cos(heading);
  const float px = -fz;
  const float pz = fx;
  const float back = length * 0.42f;
  const float wing = length * 0.58f;

  const float tipX = wx + fx * length;
  const float tipZ = wz + fz * length;
  const float leftX = wx - fx * back + px * wing;
  const float leftZ = wz - fz * back + pz * wing;
  const float rightX = wx - fx * back - px * wing;
  const float rightZ = wz - fz * back - pz * wing;

  const CellVertex tri[] = {
      {tipX, y, tipZ, r, g, b, a, 0.5f, 1.0f},
      {leftX, y, leftZ, r * 0.92f, g * 0.92f, b * 0.92f, a * 0.88f, 0.0f, 0.0f},
      {rightX, y, rightZ, r * 0.92f, g * 0.92f, b * 0.92f, a * 0.88f, 1.0f, 0.0f},
  };
  for (const CellVertex& vertex : tri) {
    verts.push_back(vertex);
  }
}

std::size_t totalOrganismFuel(const Organism& organism) {
  std::size_t total = organism.bodyStorage.size();
  for (const SkeletonNode& node : organism.nodes) {
    total += node.store.size();
  }
  return total;
}

float campBindAngleForNode(const SkeletonNode& node) {
  switch (node.neuron) {
    case NeuronType::Perceptor:
      return kCampPerceptorBindAngle;
    case NeuronType::Mouth:
      return kCampMouthBindAngle;
    case NeuronType::Actuator:
      return kCampActuatorBindAngle;
    default:
      return 0.0f;
  }
}

bool campNomVisuallyIntact(const Organism& organism) {
  if (!organism.isCampNom()) {
    return false;
  }
  for (const SkeletonNode& node : organism.nodes) {
    if (node.neuron != NeuronType::None && !node.alive) {
      return false;
    }
  }
  return true;
}

void campVisualNodePosition(const Organism& organism, const SkeletonNode& node,
                            const SkeletonNode& hub, float worldPerPx, float& outX, float& outY,
                            float& outZ) {
  outX = node.worldX;
  outY = node.worldY;
  outZ = node.worldZ;
  if (!organism.isCampNom() || !campNomVisuallyIntact(organism) ||
      node.neuron == NeuronType::Computer || node.neuron == NeuronType::None) {
    return;
  }
  const float armWorld = kNeuralAxonMaxLengthPx * worldPerPx;
  const float bindAngle = campBindAngleForNode(node);
  const float worldYaw = organism.heading + bindAngle;
  outX = hub.worldX + std::sin(worldYaw) * armWorld;
  outZ = hub.worldZ + std::cos(worldYaw) * armWorld;
}

void appendBirthFirework(std::vector<CellVertex>& verts, float eyeX, float eyeY, float eyeZ,
                         float hubX, float hubY, float hubZ, float birthHeading,
                         float worldPerPx, float parentAlpha, std::uint64_t simTick,
                         std::uint64_t celebrationStartTick, int viewportW, int viewportH) {
  if (celebrationStartTick == 0 || simTick < celebrationStartTick) {
    return;
  }

  const std::uint64_t elapsed = simTick - celebrationStartTick;
  if (elapsed >= kParthenogenesisCelebrationTicks) {
    return;
  }

  const float maxTicks = static_cast<float>(kParthenogenesisCelebrationTicks);
  const float age = static_cast<float>(elapsed) / maxTicks;
  const float fade = std::max(0.08f, 1.0f - age * age);
  const float burstAlpha = std::min(1.0f, parentAlpha * fade + 0.35f);

  const float minScreenPx = static_cast<float>(std::min(viewportW, viewportH));
  const float windowThirdWorld = worldPerPx * minScreenPx * 0.33f;
  const float burstRadius = windowThirdWorld * (0.25f + age * 0.95f);
  const float ringRadius = windowThirdWorld * (0.12f + age * 0.55f);
  const float y = hubY + 0.12f + age * 0.14f;
  const float spin = age * 1.35f;
  const float pulse = 0.85f + 0.15f * std::sin(age * kTwoPi * 3.0f);

  appendCellBillboard(verts, hubX, hubY + 0.1f, hubZ, eyeX, eyeY, eyeZ, 1.0f, 0.98f, 0.62f,
                      burstAlpha * 0.98f, windowThirdWorld * 0.14f * pulse);
  appendCellBillboard(verts, hubX, hubY + 0.14f, hubZ, eyeX, eyeY, eyeZ, 1.0f, 0.72f, 0.95f,
                      burstAlpha * 0.55f, windowThirdWorld * 0.22f * pulse);

  constexpr int kRayCount = 24;
  for (int ray = 0; ray < kRayCount; ++ray) {
    const float t = static_cast<float>(ray) / static_cast<float>(kRayCount);
    const float angle = birthHeading + t * kTwoPi + spin;
    const float dirX = std::sin(angle);
    const float dirZ = std::cos(angle);
    const float tipX = hubX + dirX * burstRadius;
    const float tipZ = hubZ + dirZ * burstRadius;

    const float hueShift = std::sin(t * kTwoPi * 3.0f + age * 8.0f) * 0.5f + 0.5f;
    const float r = 0.98f + hueShift * 0.02f;
    const float g = 0.45f + hueShift * 0.55f;
    const float b = 0.22f + (1.0f - hueShift) * 0.45f;
    const float rayAlpha = burstAlpha * (0.72f + 0.28f * (1.0f - age));
    const float halfWidth =
        worldPerPx * 4.5f * (1.0f - age * 0.25f) * (0.65f + 0.35f * pulse);

    appendGroundLinkStrip(verts, hubX, hubZ, tipX, tipZ, y, halfWidth, r, g, b, rayAlpha);

    const float midX = hubX + dirX * burstRadius * 0.55f;
    const float midZ = hubZ + dirZ * burstRadius * 0.55f;
    appendCellBillboard(verts, midX, y + 0.04f, midZ, eyeX, eyeY, eyeZ, 1.0f, 0.92f, 0.48f,
                        rayAlpha * 0.75f, worldPerPx * 14.0f * (1.0f - age * 0.4f));

    const float sparkSize = worldPerPx * 18.0f * (1.0f - age * 0.35f);
    appendCellBillboard(verts, tipX, y + 0.06f, tipZ, eyeX, eyeY, eyeZ, 1.0f, 0.88f, 0.35f,
                        rayAlpha, sparkSize);
  }

  for (int ring = 0; ring < 3; ++ring) {
    const float ringAge = std::clamp(age - static_cast<float>(ring) * 0.12f, 0.0f, 1.0f);
    if (ringAge <= 0.0f) {
      continue;
    }
    const float thisRadius = ringRadius * (0.6f + static_cast<float>(ring) * 0.35f + ringAge * 0.4f);
    constexpr int kRingSegments = 20;
    for (int seg = 0; seg < kRingSegments; ++seg) {
      const float a0 = birthHeading + (static_cast<float>(seg) / static_cast<float>(kRingSegments)) *
                                          kTwoPi +
                       spin * (0.35f + static_cast<float>(ring) * 0.2f);
      const float a1 =
          birthHeading +
          (static_cast<float>(seg + 1) / static_cast<float>(kRingSegments)) * kTwoPi +
          spin * (0.35f + static_cast<float>(ring) * 0.2f);
      const float x0 = hubX + std::sin(a0) * thisRadius;
      const float z0 = hubZ + std::cos(a0) * thisRadius;
      const float x1 = hubX + std::sin(a1) * thisRadius;
      const float z1 = hubZ + std::cos(a1) * thisRadius;
      appendGroundLinkStrip(verts, x0, z0, x1, z1, y - 0.03f * static_cast<float>(ring),
                            worldPerPx * 3.2f, 1.0f, 0.78f + static_cast<float>(ring) * 0.08f,
                            0.95f, burstAlpha * 0.55f * (1.0f - ringAge * 0.55f));
    }
  }

  const float spawnOffset = kParthenogenesisSpawnOffsetFactor * windowThirdWorld * 0.35f;
  const float spawnX = hubX + std::sin(birthHeading) * spawnOffset;
  const float spawnZ = hubZ + std::cos(birthHeading) * spawnOffset;
  appendHeadingChevron(verts, spawnX, y, spawnZ, birthHeading, windowThirdWorld * 0.28f, 1.0f,
                       0.92f, 0.38f, burstAlpha * 0.95f);
  appendHeadingChevron(verts, spawnX, y + 0.05f, spawnZ, birthHeading + 0.35f,
                       windowThirdWorld * 0.18f, 1.0f, 0.65f, 0.95f, burstAlpha * 0.7f);
}

}  // namespace

OrganismDrawBatch buildOrganismDrawBatch(const std::vector<Organism>& organisms, float eyeX,
                                         float eyeY, float eyeZ, const engine::Mat4& mvp,
                                         int viewportW, int viewportH, std::uint64_t simTick) {
  OrganismDrawBatch batch;
  batch.cellVerts.reserve(organisms.size() * 128);

  for (const Organism& organism : organisms) {
    if (!organism.alive) {
      continue;
    }

    const float fill = static_cast<float>(totalOrganismFuel(organism)) /
                       static_cast<float>(kStemCellStorageMaxBytes);
    const float alpha = 0.55f + 0.45f * std::min(1.0f, fill);

    const SkeletonNode* hub = organism.findNode(organism.rootNodeId);
    const float hubWorldPerPx =
        hub != nullptr ? worldUnitsPerPixel(mvp, viewportW, viewportH, hub->worldX, hub->worldY,
                                           hub->worldZ)
                       : worldUnitsPerPixel(mvp, viewportW, viewportH, 0.0f, 0.0f, 0.0f);

    auto visualPos = [&](const SkeletonNode& node, float& vx, float& vy, float& vz) {
      if (hub != nullptr) {
        campVisualNodePosition(organism, node, *hub, hubWorldPerPx, vx, vy, vz);
      } else {
        vx = node.worldX;
        vy = node.worldY;
        vz = node.worldZ;
      }
    };

    if (organism.isCampNom()) {
      for (const SkeletonLink& link : organism.links) {
        const SkeletonNode* parent = organism.findNode(link.parentNodeId);
        const SkeletonNode* child = organism.findNode(link.childNodeId);
        if (parent == nullptr || child == nullptr || !link.muscleBundle) {
          continue;
        }
        float px = 0.0f;
        float py = 0.0f;
        float pz = 0.0f;
        float cx = 0.0f;
        float cy = 0.0f;
        float cz = 0.0f;
        visualPos(*parent, px, py, pz);
        visualPos(*child, cx, cy, cz);
        const float worldPerPx = worldUnitsPerPixel(mvp, viewportW, viewportH, px, py, pz);
        const float halfWidth = (kNeuralAxonWidthPx * 0.5f) * worldPerPx;
        appendGroundLinkStrip(batch.cellVerts, px, pz, cx, cz, py + 0.06f, halfWidth, 0.72f, 0.86f,
                              0.34f, alpha * 0.92f);
      }
    } else {
      for (const NeuralAxon& axon : organism.neuralAxons) {
        const SkeletonNode* src = organism.findNode(axon.srcNodeId);
        const SkeletonNode* dst = organism.findNode(axon.dstNodeId);
        const bool srcDangling = axon.uncappedNodeId == axon.srcNodeId;
        const bool dstDangling = axon.uncappedNodeId == axon.dstNodeId;
        if (!srcDangling && src == nullptr) {
          continue;
        }
        if (!dstDangling && dst == nullptr) {
          continue;
        }
        float sx = 0.0f;
        float sy = 0.0f;
        float sz = 0.0f;
        float dx = 0.0f;
        float dy = 0.0f;
        float dz = 0.0f;
        if (srcDangling) {
          axonUncappedWorldPos(organism, axon, sx, sz);
          sy = (src != nullptr ? src->worldY : 0.0f) + 0.06f;
        } else {
          visualPos(*src, sx, sy, sz);
        }
        if (dstDangling) {
          axonUncappedWorldPos(organism, axon, dx, dz);
          dy = (dst != nullptr ? dst->worldY : 0.0f) + 0.06f;
        } else {
          visualPos(*dst, dx, dy, dz);
        }
        const float worldPerPx = worldUnitsPerPixel(mvp, viewportW, viewportH, sx, sy, sz);
        const float halfWidth = (kNeuralAxonWidthPx * 0.5f) * worldPerPx;
        const float segDx = dx - sx;
        const float segDz = dz - sz;
        const float segLen = std::hypot(segDx, segDz);
        const float maxAxonLen = kNeuralAxonMaxLengthPx * worldPerPx;
        float endX = dx;
        float endZ = dz;
        if (segLen > maxAxonLen + 1.0e-5f) {
          endX = sx + segDx * (maxAxonLen / segLen);
          endZ = sz + segDz * (maxAxonLen / segLen);
        }
        appendGroundLinkStrip(batch.cellVerts, sx, sz, endX, endZ, sy + 0.06f, halfWidth, 0.72f,
                              0.86f, 0.34f, alpha * 0.92f);
      }
    }

    if (const SkeletonNode* root = organism.findNode(organism.rootNodeId)) {
      const SkeletonNode* headingAnchor = root;
      if (organism.isCampNom()) {
        for (const SkeletonNode& node : organism.nodes) {
          if (node.neuron == NeuronType::Actuator) {
            headingAnchor = organism.findNode(node.id);
            break;
          }
        }
      }
      const bool showHeading =
          (root->neuron == NeuronType::Computer && organism.mouthCount() > 0) ||
          (root->neuron == NeuronType::Actuator && organism.hasActuatorNeurons()) ||
          organism.isCampNom();
      if (showHeading && headingAnchor != nullptr) {
        float ax = headingAnchor->worldX;
        float ay = headingAnchor->worldY;
        float az = headingAnchor->worldZ;
        visualPos(*headingAnchor, ax, ay, az);
        const float worldPerPx =
            worldUnitsPerPixel(mvp, viewportW, viewportH, ax, ay, az);
        const float chevronLen = kNeuralAxonMaxLengthPx * worldPerPx;
        const float strokePulse = organism.lastStrokePaid ? 1.35f : 1.0f;
        const float chevronR = organism.lastStrokePaid ? 0.55f : 0.28f;
        const float chevronG = organism.lastStrokePaid ? 1.0f : 0.98f;
        appendHeadingChevron(batch.cellVerts, ax, ay, az, organism.heading, chevronLen * strokePulse,
                             chevronR, chevronG, 1.0f,
                             alpha * (organism.lastStrokePaid ? 1.0f : 0.75f));
      }
    }

    for (const SkeletonNode& node : organism.nodes) {
      if (!node.alive) {
        continue;
      }
      float r = 0.72f;
      float g = 0.95f;
      float b = 1.0f;
      float vx = 0.0f;
      float vy = 0.0f;
      float vz = 0.0f;
      visualPos(node, vx, vy, vz);
      const float worldPerPx = worldUnitsPerPixel(mvp, viewportW, viewportH, vx, vy, vz);
      const float halfSize = (kNeuronDiameterPx * 0.5f) * worldPerPx;
      if (node.neuron == NeuronType::Mouth) {
        r = 1.0f;
        g = 0.62f;
        b = 0.28f;
      } else if (node.neuron == NeuronType::Actuator) {
        r = 0.78f;
        g = 0.62f;
        b = 0.88f;
      } else if (node.neuron == NeuronType::Perceptor) {
        r = 0.45f;
        g = 0.95f;
        b = 0.72f;
      } else if (node.neuron == NeuronType::Computer) {
        r = 0.98f;
        g = 0.92f;
        b = 0.45f;
      } else if (node.neuron == NeuronType::None) {
        r = 0.55f;
        g = 0.82f;
        b = 0.95f;
      }
      appendCellBillboard(batch.cellVerts, vx, vy, vz, eyeX, eyeY, eyeZ, r, g, b, alpha, halfSize);
    }

    if (organism.parthenogenesisCelebrationStartTick != 0 && hub != nullptr) {
      float hx = hub->worldX;
      float hy = hub->worldY;
      float hz = hub->worldZ;
      visualPos(*hub, hx, hy, hz);
      appendBirthFirework(batch.cellVerts, eyeX, eyeY, eyeZ, hx, hy, hz,
                          organism.parthenogenesisBirthHeading, hubWorldPerPx, alpha, simTick,
                          organism.parthenogenesisCelebrationStartTick, viewportW, viewportH);
    }
  }

  return batch;
}

}  // namespace evolab::game
