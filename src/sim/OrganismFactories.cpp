#include "sim/Organism.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/WorldConstants.hpp"

namespace evolab {

namespace {

void splitCampStorage(std::size_t total, std::size_t& hubBytes, std::size_t& perceptorBytes,
                      std::size_t& mouthBytes, std::size_t& actuatorBytes) {
  hubBytes = total / 2;
  const std::size_t peripheral = total - hubBytes;
  perceptorBytes = peripheral / 3;
  mouthBytes = peripheral / 3;
  actuatorBytes = peripheral - perceptorBytes - mouthBytes;
}

NeuralAxon makeDevelopmentalAxon(std::uint32_t srcId, std::uint32_t dstId) {
  NeuralAxon axon;
  axon.srcNodeId = srcId;
  axon.dstNodeId = dstId;
  setAllBelieveTrust(axon, kTrustBaseline);
  axon.trustFeed = kTrustMin;
  axon.etaEnergy = 1.0f;
  axon.etaSignal = 1.0f;
  return axon;
}

void initCampComputerRegister(Organism& organism) {
  organism.computerRegister = {kNeuronConfidenceNeutral,
                               kNeuronConfidenceNeutral,
                               kNeuronConfidenceNeutral,
                               kNeuronConfidenceNeutral,
                               1u,
                               1u,
                               1u,
                               0u};
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

Organism makeCampNomOrganism(std::uint32_t id, float wx, float wz, float wy,
                             std::size_t storageBytes, std::uint64_t createdAtTick,
                             float boneLength) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.rootNodeId = kCampRootNodeId;
  organism.computerNodeId = kCampComputerId;

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
  mouth.worldZ = wz;
  mouth.worldY = wy;

  SkeletonNode computer;
  computer.id = 3;
  computer.neuron = NeuronType::Computer;
  computer.worldX = wx;
  computer.worldZ = wz;
  computer.worldY = wy;

  SkeletonNode actuator;
  actuator.id = 4;
  actuator.neuron = NeuronType::Actuator;
  actuator.worldX = wx;
  actuator.worldZ = wz;
  actuator.worldY = wy;

  std::size_t hubBytes = 0;
  std::size_t perceptorBytes = 0;
  std::size_t mouthBytes = 0;
  std::size_t actuatorBytes = 0;
  splitCampStorage(storageBytes, hubBytes, perceptorBytes, mouthBytes, actuatorBytes);
  organism.bodyStorage.assign(hubBytes, 0);
  perceptor.store.assign(perceptorBytes, 0);
  mouth.store.assign(mouthBytes, 0);
  actuator.store.assign(actuatorBytes, 0);

  organism.nodes.push_back(perceptor);
  organism.nodes.push_back(mouth);
  organism.nodes.push_back(computer);
  organism.nodes.push_back(actuator);

  auto addCampArm = [&](std::uint32_t childId, float bindAngle) {
    SkeletonLink link;
    link.parentNodeId = kCampComputerId;
    link.childNodeId = childId;
    link.restLength = boneLength;
    link.jointAngle = bindAngle;
    link.energyEta = 0.0f;
    link.muscleBundle = true;
    organism.links.push_back(link);
  };

  addCampArm(kCampPerceptorId, kCampPerceptorBindAngle);
  addCampArm(kCampActuatorId, kCampActuatorBindAngle);
  addCampArm(kCampMouthId, kCampMouthBindAngle);

  for (const auto& edge : kCampDevelopmentalAxons) {
    organism.neuralAxons.push_back(makeDevelopmentalAxon(edge.first, edge.second));
  }

  organism.senseRadiusFactor = kPerceptorSenseRadiusFactor;
  initCampComputerRegister(organism);
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
