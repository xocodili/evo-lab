#include "engine/Camera.hpp"

#include <algorithm>
#include <cmath>

namespace evolab::engine {

Mat4 mat4Identity() { return Mat4{}; }

Mat4 mat4Perspective(float fovYRad, float aspect, float nearZ, float farZ) {
  Mat4 out{};
  const float f = 1.0f / std::tan(fovYRad * 0.5f);
  out.m[0] = f / aspect;
  out.m[5] = f;
  out.m[10] = (farZ + nearZ) / (nearZ - farZ);
  out.m[11] = -1.0f;
  out.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
  out.m[15] = 0.0f;
  return out;
}

Mat4 mat4LookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ,
                float upX, float upY, float upZ) {
  float fx = centerX - eyeX;
  float fy = centerY - eyeY;
  float fz = centerZ - eyeZ;
  const float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
  fx /= flen;
  fy /= flen;
  fz /= flen;

  float sx = fy * upZ - fz * upY;
  float sy = fz * upX - fx * upZ;
  float sz = fx * upY - fy * upX;
  const float slen = std::sqrt(sx * sx + sy * sy + sz * sz);
  sx /= slen;
  sy /= slen;
  sz /= slen;

  const float ux = sy * fz - sz * fy;
  const float uy = sz * fx - sx * fz;
  const float uz = sx * fy - sy * fx;

  Mat4 out = mat4Identity();
  out.m[0] = sx;
  out.m[1] = ux;
  out.m[2] = -fx;
  out.m[4] = sy;
  out.m[5] = uy;
  out.m[6] = -fy;
  out.m[8] = sz;
  out.m[9] = uz;
  out.m[10] = -fz;
  out.m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
  out.m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
  out.m[14] = fx * eyeX + fy * eyeY + fz * eyeZ;
  return out;
}

Mat4 mat4Multiply(const Mat4& a, const Mat4& b) {
  Mat4 out{};
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      out.m[col * 4 + row] = a.m[0 * 4 + row] * b.m[col * 4 + 0] + a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                             a.m[2 * 4 + row] * b.m[col * 4 + 2] + a.m[3 * 4 + row] * b.m[col * 4 + 3];
    }
  }
  return out;
}

void OrbitCamera::orbit(float deltaYaw, float deltaPitch) {
  yaw += deltaYaw;
  pitch += deltaPitch;
  pitch = std::clamp(pitch, 0.05f, 1.45f);
}

void OrbitCamera::zoom(float delta) {
  distance = std::clamp(distance + delta, 40.0f, 400.0f);
}

void OrbitCamera::pan(float deltaRight, float deltaForward) {
  const float forwardX = -std::sin(yaw);
  const float forwardZ = -std::cos(yaw);
  const float rightX = std::cos(yaw);
  const float rightZ = -std::sin(yaw);
  targetX += rightX * deltaRight + forwardX * deltaForward;
  targetZ += rightZ * deltaRight + forwardZ * deltaForward;
}

void OrbitCamera::eyePosition(float& eyeX, float& eyeY, float& eyeZ) const {
  const float cx = std::cos(pitch);
  eyeX = targetX + distance * cx * std::sin(yaw);
  eyeY = targetY + distance * std::sin(pitch);
  eyeZ = targetZ + distance * cx * std::cos(yaw);
}

Mat4 OrbitCamera::viewMatrix() const {
  float eyeX = 0.0f;
  float eyeY = 0.0f;
  float eyeZ = 0.0f;
  eyePosition(eyeX, eyeY, eyeZ);
  return mat4LookAt(eyeX, eyeY, eyeZ, targetX, targetY, targetZ, 0.0f, 1.0f, 0.0f);
}

}  // namespace evolab::engine
