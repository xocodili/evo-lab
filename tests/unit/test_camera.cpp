#include "engine/Camera.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;
using evolab::engine::Mat4;
using evolab::engine::mat4Identity;
using evolab::engine::mat4Multiply;

namespace {

Mat4 makeScale(float sx, float sy, float sz) {
  Mat4 m = mat4Identity();
  m.m[0] = sx;
  m.m[5] = sy;
  m.m[10] = sz;
  return m;
}

bool matricesApproxEqual(const Mat4& a, const Mat4& b, float margin = 1e-4f) {
  for (int i = 0; i < 16; ++i) {
    if (a.m[i] != Approx(b.m[i]).margin(margin)) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("mat4Multiply uses column-major order", "[camera]") {
  const Mat4 a = makeScale(2.0f, 3.0f, 4.0f);
  const Mat4 b = makeScale(5.0f, 6.0f, 7.0f);
  const Mat4 result = mat4Multiply(a, b);

  REQUIRE(result.m[0] == Approx(10.0f));
  REQUIRE(result.m[5] == Approx(18.0f));
  REQUIRE(result.m[10] == Approx(28.0f));
}

TEST_CASE("mat4Multiply with identity preserves matrix", "[camera]") {
  const Mat4 a = makeScale(1.5f, 2.5f, 3.5f);
  const Mat4 identity = mat4Identity();

  REQUIRE(matricesApproxEqual(mat4Multiply(identity, a), a));
  REQUIRE(matricesApproxEqual(mat4Multiply(a, identity), a));
}

TEST_CASE("orbit camera produces stable view matrix", "[camera]") {
  evolab::engine::OrbitCamera camera;
  camera.pitch = 0.55f;
  camera.distance = 140.0f;

  const Mat4 view = camera.viewMatrix();
  REQUIRE(std::isfinite(view.m[0]));
  REQUIRE(std::isfinite(view.m[14]));
}
