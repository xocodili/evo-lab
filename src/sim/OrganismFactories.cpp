#include "sim/Organism.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/OrganismComputer.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <random>

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

void initAllComputerNodeRegisters(Organism& organism) {
  for (SkeletonNode& node : organism.nodes) {
    if (node.neuron == NeuronType::Computer) {
      initComputerNodeRegister(node);
    }
  }
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
  initComputerNodeRegister(computer);

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
  // Mouth wallet starts empty (hungry); its spawn fuel share remains in the hub until conveyed or
  // bitten food fills the chew buffer — operational reserves are not stomach satiation.
  organism.bodyStorage.assign(hubBytes + mouthBytes, 0);
  perceptor.store.assign(perceptorBytes, 0);
  mouth.store.clear();
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
  return organism;
}

Organism makeDualComputerCampOrganism(std::uint32_t id, float wx, float wz, float wy,
                                      std::size_t storageBytes, std::uint64_t createdAtTick,
                                      float boneLength) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.rootNodeId = 3;
  organism.computerNodeId = 3;

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

  SkeletonNode computerForage;
  computerForage.id = 3;
  computerForage.neuron = NeuronType::Computer;
  computerForage.worldX = wx;
  computerForage.worldZ = wz;
  computerForage.worldY = wy;
  computerForage.computerRegister = {7u, 6u, 5u, kNeuronConfidenceNeutral, 1u, 1u, 7u, 0u};

  SkeletonNode computerThreat;
  computerThreat.id = 4;
  computerThreat.neuron = NeuronType::Computer;
  computerThreat.worldX = wx;
  computerThreat.worldZ = wz;
  computerThreat.worldY = wy;
  computerThreat.computerRegister = {1u, 2u, 3u, kNeuronConfidenceNeutral, 1u, 1u, 7u, 0u};

  SkeletonNode actuator;
  actuator.id = 5;
  actuator.neuron = NeuronType::Actuator;
  actuator.worldX = wx;
  actuator.worldZ = wz;
  actuator.worldY = wy;

  std::size_t hubBytes = 0;
  std::size_t perceptorBytes = 0;
  std::size_t mouthBytes = 0;
  std::size_t actuatorBytes = 0;
  splitCampStorage(storageBytes, hubBytes, perceptorBytes, mouthBytes, actuatorBytes);
  organism.bodyStorage.assign(hubBytes + mouthBytes, 0);
  perceptor.store.assign(perceptorBytes, 0);
  mouth.store.clear();
  actuator.store.assign(actuatorBytes, 0);

  organism.nodes.push_back(perceptor);
  organism.nodes.push_back(mouth);
  organism.nodes.push_back(computerForage);
  organism.nodes.push_back(computerThreat);
  organism.nodes.push_back(actuator);

  auto addArm = [&](std::uint32_t parentId, std::uint32_t childId, float bindAngle) {
    SkeletonLink link;
    link.parentNodeId = parentId;
    link.childNodeId = childId;
    link.restLength = boneLength;
    link.jointAngle = bindAngle;
    link.energyEta = 0.0f;
    link.muscleBundle = true;
    organism.links.push_back(link);
  };

  addArm(3, 1, kCampPerceptorBindAngle);
  addArm(3, 2, kCampMouthBindAngle);
  addArm(3, 4, 0.0f);
  addArm(4, 5, kCampActuatorBindAngle);

  static constexpr std::pair<std::uint32_t, std::uint32_t> kDualComputerAxons[] = {
      {1, 2}, {2, 1}, {1, 3}, {1, 4}, {2, 3}, {2, 4}, {5, 3}, {5, 4},
      {3, 1}, {3, 2}, {3, 4}, {3, 5}, {4, 1}, {4, 2}, {4, 3}, {4, 5},
  };
  for (const auto& edge : kDualComputerAxons) {
    organism.neuralAxons.push_back(makeDevelopmentalAxon(edge.first, edge.second));
  }

  organism.senseRadiusFactor = kPerceptorSenseRadiusFactor;
  guardComputerNodeRegister(computerForage);
  guardComputerNodeRegister(computerThreat);
  return organism;
}

Organism makeRandomCampMutant(std::uint32_t id, float wx, float wz, float wy,
                              std::size_t storageBytes, std::uint64_t createdAtTick,
                              float boneLength, std::uint64_t mutantSeed) {
  std::mt19937 rng(mutantSeed);
  const int neuronCount = 4 + static_cast<int>(rng() % 5);
  static constexpr NeuronType kPool[] = {NeuronType::Perceptor, NeuronType::Mouth,
                                         NeuronType::Computer, NeuronType::Actuator};

  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.rootNodeId = 1;
  organism.computerNodeId = 0;

  for (int i = 0; i < neuronCount; ++i) {
    SkeletonNode node;
    node.id = static_cast<std::uint32_t>(i + 1);
    node.neuron = kPool[rng() % 4];
    node.worldX = wx;
    node.worldZ = wz;
    node.worldY = wy;
    organism.nodes.push_back(node);
    if (node.neuron == NeuronType::Computer && organism.computerNodeId == 0) {
      organism.computerNodeId = node.id;
      organism.rootNodeId = node.id;
    }
  }

  for (int i = 1; i < neuronCount; ++i) {
    SkeletonLink link;
    link.parentNodeId = static_cast<std::uint32_t>(i);
    link.childNodeId = static_cast<std::uint32_t>(i + 1);
    link.restLength = boneLength;
    link.jointAngle = static_cast<float>(i) * 0.4f;
    link.energyEta = 0.5f;
    link.muscleBundle = (rng() % 2) == 0;
    organism.links.push_back(link);

    organism.neuralAxons.push_back(
        makeDevelopmentalAxon(static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(i + 1)));
    organism.neuralAxons.push_back(
        makeDevelopmentalAxon(static_cast<std::uint32_t>(i + 1), static_cast<std::uint32_t>(i)));
  }

  int perceptorCount = 0;
  int mouthCount = 0;
  int actuatorCount = 0;
  for (const SkeletonNode& node : organism.nodes) {
    switch (node.neuron) {
      case NeuronType::Perceptor:
        ++perceptorCount;
        break;
      case NeuronType::Mouth:
        ++mouthCount;
        break;
      case NeuronType::Actuator:
        ++actuatorCount;
        break;
      default:
        break;
    }
  }

  if (organism.computerNodeId != 0) {
    std::size_t hubBytes = 0;
    std::size_t perceptorBytes = 0;
    std::size_t mouthBytes = 0;
    std::size_t actuatorBytes = 0;
    splitCampStorage(storageBytes, hubBytes, perceptorBytes, mouthBytes, actuatorBytes);
    organism.bodyStorage.assign(hubBytes + mouthBytes, 0);

    int perceptorIdx = 0;
    int mouthIdx = 0;
    int actuatorIdx = 0;
    (void)perceptorIdx;
    (void)mouthIdx;
    (void)actuatorIdx;
    for (SkeletonNode& node : organism.nodes) {
      if (node.neuron == NeuronType::Perceptor) {
        const std::size_t share =
            perceptorBytes / static_cast<std::size_t>(std::max(1, perceptorCount));
        node.store.assign(share, 0);
        ++perceptorIdx;
      } else if (node.neuron == NeuronType::Mouth) {
        (void)mouthIdx;
        node.store.clear();
        ++mouthIdx;
      } else if (node.neuron == NeuronType::Actuator) {
        const std::size_t share =
            actuatorBytes / static_cast<std::size_t>(std::max(1, actuatorCount));
        node.store.assign(share, 0);
        ++actuatorIdx;
      }
    }
    initAllComputerNodeRegisters(organism);
  } else {
    organism.bodyStorage.assign(storageBytes, 0);
  }

  organism.senseRadiusFactor = kPerceptorSenseRadiusFactor;
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
  initComputerNodeRegister(root);
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
