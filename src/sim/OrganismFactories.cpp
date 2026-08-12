#include "sim/Organism.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/WorldConstants.hpp"

namespace evolab {

namespace {

void splitStorageThreeWay(std::size_t total, std::size_t& a, std::size_t& b, std::size_t& c) {
  a = total / 3;
  b = total / 3;
  c = total - a - b;
}

void capMouthFuel(std::size_t& mouthBytes, std::size_t& redistributeBytes) {
  const std::size_t cap = kMouthLocalStoreMaxBytes;
  if (mouthBytes <= cap) {
    return;
  }
  redistributeBytes += mouthBytes - cap;
  mouthBytes = cap;
}

NeuralAxon makeDevelopmentalAxon(std::uint32_t srcId, std::uint32_t dstId) {
  NeuralAxon axon;
  axon.srcNodeId = srcId;
  axon.dstNodeId = dstId;
  axon.trustBelieve = kTrustBaseline;
  axon.trustFeed = kTrustMin;
  return axon;
}

}  // namespace

Organism makeUndifferentiatedOrganism(std::uint32_t id, float wx, float wz, float wy,
                                      std::size_t storageBytes, std::uint64_t createdAtTick) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.rootNodeId = 1;
  organism.bodyStorage.resize(storageBytes);

  SkeletonNode root;
  root.id = 1;
  root.neuron = NeuronType::None;
  root.worldX = wx;
  root.worldZ = wz;
  root.worldY = wy;
  organism.nodes.push_back(root);
  return organism;
}

Organism makeActuatorOrganism(std::uint32_t id, float wx, float wz, float wy,
                              std::size_t storageBytes, std::uint64_t createdAtTick) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.rootNodeId = 1;
  organism.bodyStorage.resize(storageBytes);

  SkeletonNode root;
  root.id = 1;
  root.neuron = NeuronType::Actuator;
  root.worldX = wx;
  root.worldZ = wz;
  root.worldY = wy;
  organism.nodes.push_back(root);
  return organism;
}

Organism makeNomOrganism(std::uint32_t id, float wx, float wz, float wy, std::size_t storageBytes,
                         std::uint64_t createdAtTick, float boneLength) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.rootNodeId = 1;

  SkeletonNode perceptor;
  perceptor.id = 1;
  perceptor.neuron = NeuronType::Perceptor;
  perceptor.worldX = wx;
  perceptor.worldZ = wz;
  perceptor.worldY = wy;

  SkeletonNode mouth;
  mouth.id = 2;
  mouth.neuron = NeuronType::Mouth;
  mouth.worldX = wx;
  mouth.worldZ = wz + boneLength;
  mouth.worldY = wy;

  SkeletonNode actuator;
  actuator.id = 3;
  actuator.neuron = NeuronType::Actuator;
  actuator.worldX = wx;
  actuator.worldZ = wz + boneLength * 2.0f;
  actuator.worldY = wy;

  std::size_t perceptorBytes = 0;
  std::size_t mouthBytes = 0;
  std::size_t motorBytes = 0;
  splitStorageThreeWay(storageBytes, perceptorBytes, mouthBytes, motorBytes);
  capMouthFuel(mouthBytes, motorBytes);
  perceptor.store.assign(perceptorBytes, 0);
  mouth.store.assign(mouthBytes, 0);
  actuator.store.assign(motorBytes, 0);

  organism.nodes.push_back(perceptor);
  organism.nodes.push_back(mouth);
  organism.nodes.push_back(actuator);

  SkeletonLink perceptorToMouth;
  perceptorToMouth.parentNodeId = 1;
  perceptorToMouth.childNodeId = 2;
  perceptorToMouth.restLength = boneLength;
  perceptorToMouth.jointAngle = 0.0f;
  perceptorToMouth.energyEta = 0.0f;

  SkeletonLink mouthToActuator;
  mouthToActuator.parentNodeId = 2;
  mouthToActuator.childNodeId = 3;
  mouthToActuator.restLength = boneLength;
  mouthToActuator.jointAngle = 0.0f;
  mouthToActuator.energyEta = 0.0f;

  organism.links.push_back(perceptorToMouth);
  organism.links.push_back(mouthToActuator);

  organism.neuralAxons.push_back(makeDevelopmentalAxon(1, 2));
  organism.neuralAxons.push_back(makeDevelopmentalAxon(1, 3));
  organism.neuralAxons.push_back(makeDevelopmentalAxon(2, 3));
  organism.neuralAxons.push_back(makeDevelopmentalAxon(3, 2));

  return organism;
}

Organism makeStarMouthOrganism(std::uint32_t id, float wx, float wz, float wy,
                               std::size_t storageBytes, std::uint64_t createdAtTick,
                               int mouthCount, float boneLength) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.bodyStorage.resize(storageBytes);

  SkeletonNode root;
  root.id = 1;
  root.neuron = NeuronType::Computer;
  root.worldX = wx;
  root.worldZ = wz;
  root.worldY = wy;
  organism.rootNodeId = root.id;
  organism.nodes.push_back(root);

  const int spokes = std::max(1, mouthCount);
  for (int i = 0; i < spokes; ++i) {
    const float angle = static_cast<float>(i) * 6.2831853f / static_cast<float>(spokes);
    SkeletonNode mouth;
    mouth.id = static_cast<std::uint32_t>(2 + i);
    mouth.neuron = NeuronType::Mouth;

    SkeletonLink link;
    link.parentNodeId = root.id;
    link.childNodeId = mouth.id;
    link.restLength = boneLength;
    link.jointAngle = angle;
    link.energyEta = 0.88f;

    organism.nodes.push_back(mouth);
    organism.links.push_back(link);
  }

  return organism;
}

Organism makeTwoMouthOrganism(std::uint32_t id, float wx, float wz, float wy,
                              std::size_t storageBytes, std::uint64_t createdAtTick,
                              float boneLength, std::uint16_t trustFeedM1ToM2,
                              std::uint16_t trustFeedM2ToM1) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.bodyStorage.resize(storageBytes);
  organism.rootNodeId = 1;

  SkeletonNode mouthA;
  mouthA.id = 1;
  mouthA.neuron = NeuronType::Mouth;
  mouthA.worldX = wx;
  mouthA.worldZ = wz;
  mouthA.worldY = wy;

  SkeletonNode mouthB;
  mouthB.id = 2;
  mouthB.neuron = NeuronType::Mouth;
  mouthB.worldX = wx;
  mouthB.worldZ = wz + boneLength;
  mouthB.worldY = wy;

  organism.nodes.push_back(mouthA);
  organism.nodes.push_back(mouthB);

  SkeletonLink bone;
  bone.parentNodeId = 1;
  bone.childNodeId = 2;
  bone.restLength = boneLength;
  bone.jointAngle = 0.0f;
  bone.energyEta = 0.0f;
  organism.links.push_back(bone);

  NeuralAxon axonAtoB;
  axonAtoB.srcNodeId = 1;
  axonAtoB.dstNodeId = 2;
  axonAtoB.trustBelieve = kTrustBaseline;
  axonAtoB.trustFeed = trustFeedM1ToM2;

  NeuralAxon axonBtoA;
  axonBtoA.srcNodeId = 2;
  axonBtoA.dstNodeId = 1;
  axonBtoA.trustBelieve = kTrustBaseline;
  axonBtoA.trustFeed = trustFeedM2ToM1;

  organism.neuralAxons.push_back(axonAtoB);
  organism.neuralAxons.push_back(axonBtoA);

  return organism;
}

bool organismLandAdjacent(const BarrenWorld& world, float wx, float wz, float cellSize) {
  const float terrainHeight = world.heightAtWorld(wx, wz, cellSize);
  const float waterLevel = world.effectiveWaterLevelAt(wx, wz, cellSize);
  if (terrainHeight >= waterLevel) {
    return true;
  }
  const float eps = 0.05f;
  return terrainHeight >= waterLevel - eps;
}
}

