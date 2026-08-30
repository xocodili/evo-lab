#include "engine/gfx/sprites/SpriteBillboardBuilder.hpp"

#include <cmath>

namespace evolab::engine::gfx::sprites {

namespace {

void buildBillboardBasis(float wx, float wy, float wz, float eyeX, float eyeY, float eyeZ,
                         float& rightX, float& rightY, float& rightZ, float& upX, float& upY,
                         float& upZ) {
  float toEyeX = eyeX - wx;
  float toEyeY = eyeY - wy;
  float toEyeZ = eyeZ - wz;
  const float toEyeLen = std::sqrt(toEyeX * toEyeX + toEyeY * toEyeY + toEyeZ * toEyeZ);
  if (toEyeLen <= 0.0001f) {
    rightX = 1.0f;
    rightY = 0.0f;
    rightZ = 0.0f;
    upX = 0.0f;
    upY = 1.0f;
    upZ = 0.0f;
    return;
  }
  toEyeX /= toEyeLen;
  toEyeY /= toEyeLen;
  toEyeZ /= toEyeLen;

  rightX = toEyeZ;
  rightY = 0.0f;
  rightZ = -toEyeX;
  float rightLen = std::sqrt(rightX * rightX + rightZ * rightZ);
  if (rightLen <= 0.0001f) {
    rightX = 1.0f;
    rightZ = 0.0f;
    rightLen = 1.0f;
  }
  rightX /= rightLen;
  rightZ /= rightLen;

  upX = rightY * toEyeZ - rightZ * toEyeY;
  upY = rightZ * toEyeX - rightX * toEyeZ;
  upZ = rightX * toEyeY - rightY * toEyeX;
}

}  // namespace

void appendSpriteBillboard(std::vector<SpriteVertex>& out, const SpriteDrawInstance& instance,
                           const SpriteFrame& frame, const SpriteAtlas& atlas, float eyeX,
                           float eyeY, float eyeZ) {
  if (instance.halfSizeWorld <= 0.0f || atlas.widthPx <= 0 || atlas.heightPx <= 0) {
    return;
  }

  float rightX = 0.0f;
  float rightY = 0.0f;
  float rightZ = 0.0f;
  float upX = 0.0f;
  float upY = 0.0f;
  float upZ = 0.0f;
  buildBillboardBasis(instance.worldX, instance.worldY, instance.worldZ, eyeX, eyeY, eyeZ, rightX,
                      rightY, rightZ, upX, upY, upZ);

  const float aspect =
      frame.h > 0 ? static_cast<float>(frame.w) / static_cast<float>(frame.h) : 1.0f;
  const float halfW = instance.halfSizeWorld * aspect;
  const float halfH = instance.halfSizeWorld;
  const float flip = instance.flipX != 0 ? -1.0f : 1.0f;

  const SpriteUvRect uv = atlas.uvForFrame(frame);
  const struct Corner {
    float lx, ly, u, v;
  } corners[] = {
      {-1.0f, -1.0f, uv.u0, uv.v1},
      {1.0f, -1.0f, uv.u1, uv.v1},
      {1.0f, 1.0f, uv.u1, uv.v0},
      {-1.0f, 1.0f, uv.u0, uv.v0},
  };

  SpriteVertex cornerVerts[4];
  int cornerIdx = 0;
  for (const Corner& corner : corners) {
    const float lx = corner.lx * flip;
    const float ly = corner.ly;
    const float px = instance.worldX + rightX * lx * halfW + upX * ly * halfH;
    const float py = instance.worldY + rightY * lx * halfW + upY * ly * halfH;
    const float pz = instance.worldZ + rightZ * lx * halfW + upZ * ly * halfH;
    cornerVerts[static_cast<std::size_t>(cornerIdx++)] =
        SpriteVertex{px, py, pz, instance.tintR, instance.tintG, instance.tintB, instance.tintA,
                     lx, ly, corner.u, corner.v};
  }

  // Two triangles (batch-safe; avoid triangle-fan merging across instances).
  const int tri[] = {0, 1, 2, 0, 2, 3};
  for (int idx : tri) {
    out.push_back(cornerVerts[static_cast<std::size_t>(idx)]);
  }
}

}  // namespace evolab::engine::gfx::sprites
