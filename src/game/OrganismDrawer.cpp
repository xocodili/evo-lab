#include "game/OrganismDrawer.hpp"

#include "sim/CellConstants.hpp"
#include "sim/Organism.hpp"

#include <algorithm>
#include <cmath>

namespace evolab::game {

namespace {

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

void pushLineVertex(std::vector<OrganismLineVertex>& out, float x, float y, float z, float r,
                    float g, float b, float a) {
  out.push_back({x, y, z, r, g, b, a});
}

void appendLinkLine(std::vector<OrganismLineVertex>& verts, const SkeletonNode& a,
                    const SkeletonNode& b, float yOffset, float r, float g, float colB,
                    float alpha, float maxLength) {
  if (!linkWorldDistanceOk(a, b, maxLength)) {
    return;
  }
  const float y = std::max(a.worldY, b.worldY) + yOffset;
  pushLineVertex(verts, a.worldX, y, a.worldZ, r, g, colB, alpha);
  pushLineVertex(verts, b.worldX, y, b.worldZ, r * 0.95f, g * 0.95f, colB * 0.95f, alpha * 0.9f);
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

void appendHeadingChevron(std::vector<CellVertex>& verts, float wx, float wy, float wz, float heading,
                          float length, float r, float g, float b, float a) {
  const float y = wy + 0.14f;
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

}  // namespace

OrganismDrawBatch buildOrganismDrawBatch(const std::vector<Organism>& organisms, float eyeX,
                                         float eyeY, float eyeZ) {
  OrganismDrawBatch batch;
  batch.cellVerts.reserve(organisms.size() * 48);
  batch.boneLineVerts.reserve(organisms.size() * 12);
  batch.neuralLineVerts.reserve(organisms.size() * 24);

  for (const Organism& organism : organisms) {
    if (!organism.alive) {
      continue;
    }

    const float fill = static_cast<float>(totalOrganismFuel(organism)) /
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
      appendGroundBoneStrip(batch.cellVerts, *parent, *child, 0.98f, 0.88f, 0.22f, alpha * 0.95f,
                            0.16f, link.restLength);
      appendLinkLine(batch.boneLineVerts, *parent, *child, 0.32f, 1.0f, 0.92f, 0.15f, alpha,
                     link.restLength);
    }

    for (const NeuralAxon& axon : organism.neuralAxons) {
      const SkeletonNode* src = organism.findNode(axon.srcNodeId);
      const SkeletonNode* dst = organism.findNode(axon.dstNodeId);
      if (src != nullptr && dst != nullptr) {
        appendLinkLine(batch.neuralLineVerts, *src, *dst, 0.42f, 0.75f, 0.25f, 1.0f, alpha * 0.95f,
                       maxBoneLen > 0.0f ? maxBoneLen : 1.2f);
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
        const float chevronLen = maxBoneLen > 0.0f ? maxBoneLen * 0.55f : 0.42f;
        const float strokePulse = organism.lastStrokePaid ? 1.75f : 1.0f;
        const float chevronR = organism.lastStrokePaid ? 0.55f : 0.28f;
        const float chevronG = organism.lastStrokePaid ? 1.0f : 0.98f;
        appendHeadingChevron(batch.cellVerts, headingAnchor->worldX, headingAnchor->worldY,
                             headingAnchor->worldZ, organism.heading, chevronLen * strokePulse,
                             chevronR, chevronG, 1.0f,
                             alpha * (organism.lastStrokePaid ? 1.0f : 0.92f));
      }
    }

    for (const SkeletonNode& node : organism.nodes) {
      if (!node.alive) {
        continue;
      }
      float r = 0.72f;
      float g = 0.95f;
      float b = 1.0f;
      float halfSize = 0.16f;
      if (node.neuron == NeuronType::Mouth) {
        r = 1.0f;
        g = 0.62f;
        b = 0.28f;
        halfSize = 0.13f;
      } else if (node.neuron == NeuronType::Actuator) {
        r = 0.78f;
        g = 0.62f;
        b = 0.88f;
        halfSize = 0.15f;
      } else if (node.neuron == NeuronType::Perceptor) {
        r = 0.45f;
        g = 0.95f;
        b = 0.72f;
        halfSize = 0.14f;
      } else if (node.neuron == NeuronType::Computer) {
        r = 0.98f;
        g = 0.92f;
        b = 0.45f;
        halfSize = 0.17f;
      } else if (node.neuron == NeuronType::None) {
        r = 0.55f;
        g = 0.82f;
        b = 0.95f;
        halfSize = 0.12f;
      }
      appendCellBillboard(batch.cellVerts, node.worldX, node.worldY, node.worldZ, eyeX, eyeY, eyeZ,
                          r, g, b, alpha, halfSize);
    }
  }

  return batch;
}

}  // namespace evolab::game
