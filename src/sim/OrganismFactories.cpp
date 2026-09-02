#include "sim/Organism.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/OrganismComputer.hpp"
#include "sim/NeuronCoordinator.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/StemBinding.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <random>

namespace evolab {

namespace {

void initAllComputerNodeRegisters(Organism& organism) {
  for (SkeletonNode& node : organism.nodes) {
    initCoordinatorNodeRegister(node);
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

  SkeletonNode root;
  root.id = 1;
  root.neuron = NeuronType::None;
  root.worldX = wx;
  root.worldZ = wz;
  root.worldY = wy;
  initStemNodeStore(root, storageBytes);
  organism.nodes.push_back(root);
  initCoordinatorNodeRegister(organism.nodes.back());
  return organism;
}

Organism makeActuatorOrganism(std::uint32_t id, float wx, float wz, float wy,
                              std::size_t storageBytes, std::uint64_t createdAtTick) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.rootNodeId = 1;

  SkeletonNode root;
  root.id = 1;
  root.neuron = NeuronType::Actuator;
  root.worldX = wx;
  root.worldZ = wz;
  root.worldY = wy;
  initStemNodeStore(root, storageBytes);
  organism.nodes.push_back(root);
  return organism;
}

Organism makeCampNomOrganism(std::uint32_t id, float wx, float wz, float wy,
                             std::size_t storageBytes, std::uint64_t createdAtTick,
                             float boneLength) {
  return assembleOrganismFromStemPlan(id, wx, wz, wy, storageBytes, createdAtTick, boneLength,
                                      defaultCampStemAssemblyPlan(), 0.0f);
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
  const CampStorageSplit split = splitCampStorage(storageBytes);
  hubBytes = split.hubBytes;
  perceptorBytes = split.perceptorBytes;
  mouthBytes = split.mouthBytes;
  actuatorBytes = split.actuatorBytes;
  initComputerHubStore(computerForage, hubBytes + mouthBytes, organism);
  initPeripheralNodeStore(perceptor, perceptorBytes, organism);
  mouth.store.clear();
  initPeripheralNodeStore(actuator, actuatorBytes, organism);

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
  for (SkeletonNode& node : organism.nodes) {
    initCoordinatorNodeRegister(node);
  }
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

  if (organism.computerNodeId != 0) {
    endowCampNodesFromSplit(organism, splitCampStorage(storageBytes));
    initAllComputerNodeRegisters(organism);
  } else {
    SkeletonNode* root = organism.findNode(organism.rootNodeId);
    if (root != nullptr) {
      initStemNodeStore(*root, storageBytes);
    }
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

  SkeletonNode root;
  root.id = 1;
  root.neuron = NeuronType::Computer;
  root.worldX = wx;
  root.worldZ = wz;
  root.worldY = wy;
  initComputerHubStore(root, storageBytes, organism);
  organism.rootNodeId = root.id;
  organism.computerNodeId = root.id;
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

  initAllComputerNodeRegisters(organism);
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

void ensureCampDevelopmentalAxons(Organism& organism) {
  if (!organismHasCampNeuronFloor(organism)) {
    return;
  }
  for (const auto& edge : kCampDevelopmentalAxons) {
    if (organism.findNeuralAxon(edge.first, edge.second) != nullptr) {
      continue;
    }
    if (organism.neuralAxons.size() >= kAxonChannelCapacity) {
      break;
    }
    organism.neuralAxons.push_back(makeDevelopmentalAxon(edge.first, edge.second));
  }
}

}
