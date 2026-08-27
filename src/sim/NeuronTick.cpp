#include "sim/NeuronTick.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/Energon.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismInternal.hpp"
#include "sim/TideAdvection.hpp"

namespace evolab {

namespace {

enum class LocomotionMode : std::uint8_t { PassiveDrift, Actuator };

LocomotionMode locomotionMode(const Organism& organism) {
  if (organism.hasLiveActuatorNeurons()) {
    return LocomotionMode::Actuator;
  }
  return LocomotionMode::PassiveDrift;
}

void advectPassiveDrift(Organism& organism, const OrganismTickContext& ctx,
                        const AdvectionVelocity& velocity) {
  SkeletonNode* root = organism.findNode(organism.rootNodeId);
  if (root == nullptr) {
    return;
  }
  organism_detail::updateOrganismHeading(organism, velocity, ctx.energon, ctx.cellSize);
  applyShoreAdvection(root->worldX, root->worldZ, velocity, ctx.halfExtent, ctx.cellSize * 0.25f);
}

void finalizeAdvectPose(Organism& organism, const OrganismTickContext& ctx) {
  organism.updateKinematics(ctx.world, ctx.cellSize, ctx.heightScale);
  SkeletonNode* root = organism.findNode(organism.rootNodeId);
  if (root == nullptr) {
    return;
  }
  clampWorldPosition(root->worldX, root->worldZ, ctx.halfExtent, ctx.cellSize * 0.25f);
  organism.updateKinematics(ctx.world, ctx.cellSize, ctx.heightScale);
  organism.landAdjacent =
      organismLandAdjacent(ctx.world, root->worldX, root->worldZ, ctx.cellSize);
}

}  // namespace

void runOrganismPreAdvectHooks(Organism& organism, const OrganismTickContext& ctx) {
  if (!organism.alive) {
    return;
  }
  if (organism.isCampNom()) {
    organism.emitPreAdvectSignals(ctx.simTick);
  }
  // Perceptor (P) pre-advect hooks register here.
}

void runOrganismAdvect(Organism& organism, const OrganismTickContext& ctx) {
  if (!organism.alive) {
    return;
  }
  SkeletonNode* root = organism.findNode(organism.rootNodeId);
  if (root == nullptr) {
    return;
  }

  const AdvectionVelocity velocity =
      shoreAdvection(ctx.world, root->worldX, root->worldZ, ctx.cellSize, ctx.halfExtent);

  switch (locomotionMode(organism)) {
    case LocomotionMode::Actuator:
      organism_detail::tickActuatorOrganism(organism, ctx.world, ctx.cellSize, ctx.halfExtent,
                                            ctx.simTick);
      break;
    case LocomotionMode::PassiveDrift:
      advectPassiveDrift(organism, ctx, velocity);
      break;
  }

  finalizeAdvectPose(organism, ctx);
}

}  // namespace evolab
