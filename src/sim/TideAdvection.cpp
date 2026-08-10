#include "sim/TideAdvection.hpp"



#include "sim/BarrenWorld.hpp"



#include <algorithm>

#include <cmath>



namespace evolab {



namespace {



struct Vec2 {

  float x = 0.0f;

  float z = 0.0f;

};



Vec2 depthGradient(const BarrenWorld& world, float wx, float wz, float cellSize) {

  const float sampleStep = std::max(cellSize * 0.45f, 0.25f);

  const float dXp = world.depthAtWorld(wx + sampleStep, wz, cellSize);

  const float dXm = world.depthAtWorld(wx - sampleStep, wz, cellSize);

  const float dZp = world.depthAtWorld(wx, wz + sampleStep, cellSize);

  const float dZm = world.depthAtWorld(wx, wz - sampleStep, cellSize);

  return {(dXp - dXm) * 0.5f / sampleStep, (dZp - dZm) * 0.5f / sampleStep};

}



bool normalize(Vec2& v) {

  const float len = std::sqrt(v.x * v.x + v.z * v.z);

  if (len <= 1.0e-5f) {

    v.x = 0.0f;

    v.z = 0.0f;

    return false;

  }

  v.x /= len;

  v.z /= len;

  return true;

}



Vec2 blendFlow(Vec2 primary, Vec2 secondary, float secondaryWeight) {

  if (secondaryWeight <= 0.0f) {

    return primary;

  }

  const float primaryWeight = 1.0f - secondaryWeight;

  Vec2 blended{primary.x * primaryWeight + secondary.x * secondaryWeight,

               primary.z * primaryWeight + secondary.z * secondaryWeight};

  if (!normalize(blended)) {

    return primary;

  }

  return blended;

}



// Unit vector pointing from the nearest map edge toward the interior.

Vec2 boundaryInward(float wx, float wz, float halfExtent) {

  if (halfExtent <= 0.0f) {

    return {};

  }



  const float distPosX = halfExtent - wx;

  const float distNegX = halfExtent + wx;

  const float distPosZ = halfExtent - wz;

  const float distNegZ = halfExtent + wz;

  const float minDist = std::min({distPosX, distNegX, distPosZ, distNegZ});

  if (minDist <= 1.0e-4f) {

    if (minDist == distPosX) {

      return {-1.0f, 0.0f};

    }

    if (minDist == distNegX) {

      return {1.0f, 0.0f};

    }

    if (minDist == distPosZ) {

      return {0.0f, -1.0f};

    }

    return {0.0f, 1.0f};

  }



  Vec2 inward{0.0f, 0.0f};

  if (minDist == distPosX) {

    inward.x = -1.0f;

  } else if (minDist == distNegX) {

    inward.x = 1.0f;

  } else if (minDist == distPosZ) {

    inward.z = -1.0f;

  } else {

    inward.z = 1.0f;

  }

  normalize(inward);

  return inward;

}



float boundaryProximity(float wx, float wz, float halfExtent, float bandWidth) {

  if (halfExtent <= 0.0f || bandWidth <= 0.0f) {

    return 0.0f;

  }

  const float distToEdge = std::min(halfExtent - std::abs(wx), halfExtent - std::abs(wz));

  return 1.0f - std::clamp(distToEdge / bandWidth, 0.0f, 1.0f);

}



}  // namespace



AdvectionVelocity shoreAdvection(const BarrenWorld& world, float worldX, float worldZ, float cellSize,

                                 float worldHalfExtent, const TideAdvectionConfig& config) {

  AdvectionVelocity out;

  const float deltaLevel = world.waterLevelDelta();

  if (std::abs(deltaLevel) <= 1.0e-5f) {

    return out;

  }



  const float terrainHeight = world.heightAtWorld(worldX, worldZ, cellSize);

  const float waterSurface = world.effectiveWaterLevelAt(worldX, worldZ, cellSize);

  const float depth = waterSurface - terrainHeight;

  const bool wet = depth > 0.0f;

  const bool connected = world.isHydraulicallyConnectedAt(worldX, worldZ, cellSize);

  const bool impounded = world.isImpoundedAt(worldX, worldZ, cellSize);

  const bool dryEdge = !wet && (terrainHeight - waterSurface) <= config.dryMarginDepth;



  if (impounded) {

    return out;

  }



  if (!connected) {

    if (!(dryEdge && deltaLevel > 0.0f)) {

      return out;

    }

  }



  if (!wet && !dryEdge) {

    return out;

  }



  const FlowVector drainage = world.flowDirectionAtWorld(worldX, worldZ, cellSize);

  Vec2 flow{0.0f, 0.0f};

  bool haveFlow = false;



  if (drainage.valid) {

    if (deltaLevel < 0.0f) {

      flow = {drainage.dx, drainage.dz};

    } else {

      flow = {-drainage.dx, -drainage.dz};

    }

    haveFlow = true;

  }



  if (wet && deltaLevel < 0.0f) {

    Vec2 gradDepth = depthGradient(world, worldX, worldZ, cellSize);

    if (normalize(gradDepth)) {

      if (haveFlow) {

        flow = blendFlow(flow, gradDepth, config.poolFlowBlend);

      } else {

        flow = gradDepth;

        haveFlow = true;

      }

    }

  } else if (wet && deltaLevel > 0.0f) {

    Vec2 gradDepth = depthGradient(world, worldX, worldZ, cellSize);

    Vec2 spread{-gradDepth.x, -gradDepth.z};

    if (normalize(spread)) {

      if (haveFlow) {

        flow = blendFlow(flow, spread, config.poolFlowBlend * 0.5f);

      } else {

        flow = spread;

        haveFlow = true;

      }

    }

  }



  if (deltaLevel > 0.0f && worldHalfExtent > 0.0f) {

    const float edgeWeight =

        boundaryProximity(worldX, worldZ, worldHalfExtent, cellSize * 2.5f) * config.boundaryInflowWeight;

    if (edgeWeight > 0.0f) {

      Vec2 inward = boundaryInward(worldX, worldZ, worldHalfExtent);

      if (normalize(inward)) {

        if (haveFlow) {

          flow = blendFlow(flow, inward, edgeWeight);

        } else {

          flow = inward;

          haveFlow = true;

        }

      }

    }

  }



  if (!haveFlow || !normalize(flow)) {

    return out;

  }



  const float speed = std::min(config.maxStepPerTick, config.speedScale * std::abs(deltaLevel));

  out.vx = flow.x * speed;

  out.vz = flow.z * speed;

  out.active = true;

  return out;

}



void clampWorldPosition(float& worldX, float& worldZ, float worldHalfExtent, float boundaryMargin) {
  const float limit = worldHalfExtent - std::max(boundaryMargin, 0.0f);
  if (limit > 0.0f) {
    worldX = std::clamp(worldX, -limit, limit);
    worldZ = std::clamp(worldZ, -limit, limit);
  } else if (worldHalfExtent > 0.0f) {
    worldX = std::clamp(worldX, -worldHalfExtent, worldHalfExtent);
    worldZ = std::clamp(worldZ, -worldHalfExtent, worldHalfExtent);
  }
}



void applyShoreAdvection(float& worldX, float& worldZ, const AdvectionVelocity& velocity,

                         float worldHalfExtent, float boundaryMargin) {

  if (!velocity.active) {

    return;

  }



  const float limit = worldHalfExtent - std::max(boundaryMargin, 0.0f);

  float vx = velocity.vx;

  float vz = velocity.vz;



  if (limit > 0.0f) {

    if (worldX <= -limit + 1.0e-3f && vx < 0.0f) {

      vx = 0.0f;

    }

    if (worldX >= limit - 1.0e-3f && vx > 0.0f) {

      vx = 0.0f;

    }

    if (worldZ <= -limit + 1.0e-3f && vz < 0.0f) {

      vz = 0.0f;

    }

    if (worldZ >= limit - 1.0e-3f && vz > 0.0f) {

      vz = 0.0f;

    }

  }



  worldX += vx;

  worldZ += vz;

  clampWorldPosition(worldX, worldZ, worldHalfExtent, boundaryMargin);

}



}  // namespace evolab

