#include "sim/Organism.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/Energon.hpp"
#include "sim/NeuronTick.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/OrganismInternal.hpp"
#include "sim/OrganismMouth.hpp"
#include "sim/EnergonConveyance.hpp"
#include "sim/NeuronTrust.hpp"
#include "sim/OrganismPerceptor.hpp"

#include <algorithm>

namespace evolab {

SkeletonNode* Organism::findNode(std::uint32_t nodeId) {
  for (SkeletonNode& node : nodes) {
    if (node.id == nodeId) {
      return &node;
    }
  }
  return nullptr;
}

const SkeletonNode* Organism::findNode(std::uint32_t nodeId) const {
  for (const SkeletonNode& node : nodes) {
    if (node.id == nodeId) {
      return &node;
    }
  }
  return nullptr;
}

NeuralAxon* Organism::findNeuralAxon(std::uint32_t srcNodeId, std::uint32_t dstNodeId) {
  for (NeuralAxon& axon : neuralAxons) {
    if (axon.srcNodeId == srcNodeId && axon.dstNodeId == dstNodeId) {
      return &axon;
    }
  }
  return nullptr;
}

const NeuralAxon* Organism::findNeuralAxon(std::uint32_t srcNodeId,
                                           std::uint32_t dstNodeId) const {
  for (const NeuralAxon& axon : neuralAxons) {
    if (axon.srcNodeId == srcNodeId && axon.dstNodeId == dstNodeId) {
      return &axon;
    }
  }
  return nullptr;
}

float Organism::rootWorldX() const {
  if (const SkeletonNode* root = findNode(rootNodeId)) {
    return root->worldX;
  }
  return 0.0f;
}

float Organism::rootWorldY() const {
  if (const SkeletonNode* root = findNode(rootNodeId)) {
    return root->worldY;
  }
  return 0.0f;
}

float Organism::rootWorldZ() const {
  if (const SkeletonNode* root = findNode(rootNodeId)) {
    return root->worldZ;
  }
  return 0.0f;
}

void Organism::advectRoot(const BarrenWorld& world, const EnergonField& energon, float cellSize,
                          float heightScale, float halfExtent) {
  const OrganismTickContext ctx{world, energon, cellSize, heightScale, halfExtent,
                                world.tickCount()};
  runOrganismAdvect(*this, ctx);
}

void Organism::metabolise(const BarrenWorld& world, float cellSize, float heightScale) {
  if (!alive) {
    return;
  }

  updateKinematics(world, cellSize, heightScale);
}

void Organism::tickNeuronViability(EnergonField& field) {
  organism_detail::tickNeuronViability(*this, field);
}

void Organism::feed(EnergonField& field, float cellSize, std::uint64_t simTick) {
  if (!alive) {
    return;
  }

  for (SkeletonNode& node : nodes) {
    node.ateThisTick = false;
  }
  lastMouthBiteDrive = 0.0f;
  lastMouthFeedSuppressed = false;
  lastMouthHadFoodContact = false;

  const FeedIntent* pmaFeedIntent = nullptr;
  FeedIntent feedIntent{};
  MouthInteroception mouthInteroception{};
  std::uint32_t pmaMouthId = 0;
  if (isPmaNom()) {
    for (SkeletonNode& node : nodes) {
      if (!node.alive || node.neuron != NeuronType::Mouth) {
        continue;
      }
      mouthInteroception = gatherMouthInteroception(*this, node.id, node, simTick);
      feedIntent = computePmaFeedIntent(mouthInteroception);
      lastMouthBiteDrive = feedIntent.biteDrive;
      lastMouthFeedSuppressed = feedIntent.feedSuppressed;
      pmaFeedIntent = &feedIntent;
      pmaMouthId = node.id;
      break;
    }
  }

  const float radius = cellSize * kMouthContactRadiusFactor;
  for (SkeletonNode& node : nodes) {
    if (node.alive && node.neuron == NeuronType::Mouth) {
      organism_detail::tickMouthNode(*this, node, field, radius, simTick, pmaFeedIntent);
    }
  }

  if (isPmaNom() && pmaMouthId != 0) {
    MouthTrustEvent trustEvent;
    trustEvent.hadFoodContact = lastMouthHadFoodContact;
    trustEvent.ate = false;
    for (const SkeletonNode& node : nodes) {
      if (node.alive && node.neuron == NeuronType::Mouth && node.ateThisTick) {
        trustEvent.ate = true;
        break;
      }
    }
    trustEvent.feedSuppressed = lastMouthFeedSuppressed;
    applyPmaMouthTrustLearning(*this, pmaMouthId, trustEvent, simTick);
  }
}

void Organism::perceive(const BarrenWorld& world, const EnergonField& energon, float cellSize,
                        float halfExtent, const std::vector<Organism>& population,
                        std::uint64_t simTick, float sunIntensity) {
  runPerceptorPhase(*this, world, energon, cellSize, halfExtent, population, simTick,
                    sunIntensity);
}

void Organism::transferEnergy(EnergonField& field, float cellSize, std::uint64_t simTick) {
  (void)field;
  (void)cellSize;
  if (!alive) {
    return;
  }

  if (isPmaNom() && hasNeuralAxons()) {
    conveyPmaEnergon(*this, field, simTick);
    return;
  }

  if (hasNeuralAxons()) {
    return;
  }

  SkeletonNode* root = findNode(rootNodeId);
  if (root == nullptr) {
    return;
  }

  for (const SkeletonLink& link : links) {
    if (link.energyEta <= 0.0f) {
      continue;
    }
    SkeletonNode* child = findNode(link.childNodeId);
    SkeletonNode* parent = findNode(link.parentNodeId);
    if (child == nullptr || parent == nullptr || child->store.empty()) {
      continue;
    }

    const std::size_t moveCount =
        std::max<std::size_t>(1, static_cast<std::size_t>(static_cast<float>(child->store.size()) *
                                                          link.energyEta));
    const std::size_t actualMove = std::min(moveCount, child->store.size());

    if (parent->id == rootNodeId) {
      for (std::size_t i = 0; i < actualMove; ++i) {
        if (bodyStorage.size() >= kStemCellStorageMaxBytes) {
          break;
        }
        bodyStorage.push_back(child->store.back());
        child->store.pop_back();
      }
    } else {
      for (std::size_t i = 0; i < actualMove; ++i) {
        if (parent->store.size() >= kMouthLocalStoreMaxBytes) {
          break;
        }
        parent->store.push_back(child->store.back());
        child->store.pop_back();
      }
    }
  }
}

void Organism::signal(EnergonField& field, std::uint64_t simTick) {
  if (!alive || neuralAxons.empty()) {
    return;
  }
  organism_detail::runMouthSignalPhase(*this, field, simTick);
}

void Organism::transferColony() {}

int Organism::mouthCount() const {
  int count = 0;
  for (const SkeletonNode& node : nodes) {
    if (node.neuron == NeuronType::Mouth) {
      ++count;
    }
  }
  return count;
}

bool Organism::hasMouthNeurons() const {
  return mouthCount() > 0;
}

int Organism::perceptorCount() const {
  int count = 0;
  for (const SkeletonNode& node : nodes) {
    if (node.neuron == NeuronType::Perceptor) {
      ++count;
    }
  }
  return count;
}

bool Organism::hasPerceptorNeurons() const {
  return perceptorCount() > 0;
}

int Organism::actuatorCount() const {
  int count = 0;
  for (const SkeletonNode& node : nodes) {
    if (node.neuron == NeuronType::Actuator) {
      ++count;
    }
  }
  return count;
}

bool Organism::hasLiveActuatorNeurons() const {
  for (const SkeletonNode& node : nodes) {
    if (node.alive && node.neuron == NeuronType::Actuator) {
      return true;
    }
  }
  return false;
}

bool Organism::hasLiveFunctionalNeurons() const {
  if (!alive) {
    return false;
  }
  for (const SkeletonNode& node : nodes) {
    if (!node.alive) {
      continue;
    }
    if (node.neuron != NeuronType::None) {
      return true;
    }
    if (node.id == rootNodeId && nodes.size() == 1) {
      return true;
    }
  }
  return false;
}

bool Organism::hasActuatorNeurons() const {
  return actuatorCount() > 0;
}

bool Organism::isPmaNom() const {
  return organismHasPmaTopology(*this);
}

void Organism::emitPreAdvectSignals(std::uint64_t simTick) {
  if (!isPmaNom()) {
    return;
  }
  organism_detail::emitMouthConfidenceSignals(*this, simTick);
}

bool Organism::hasNeuralAxons() const {
  return !neuralAxons.empty();
}

void Organism::finalizeSpawn(std::mt19937& rng) {
  for (NeuralAxon& axon : neuralAxons) {
    initializeDevelopmentalAxonTrust(axon, rng);
  }

  heading = chaosJitterHeading(heading, rng);

  senseRadiusFactor = chaosJitterFloat(kPerceptorSenseRadiusFactor, rng);

  for (SkeletonLink& link : links) {
    link.restLength = chaosJitterFloat(link.restLength, rng);
    link.jointAngle = chaosJitterFloat(link.jointAngle, rng);
    link.energyEta = chaosJitterFloat(link.energyEta, rng);
  }
}

void Organism::pruneNeuralAxons() {
  neuralAxons.erase(std::remove_if(neuralAxons.begin(), neuralAxons.end(), axonMarkedForPruning),
                    neuralAxons.end());
}

bool Organism::allLocalStoresEmpty() const {
  for (const SkeletonNode& node : nodes) {
    if (!node.store.empty()) {
      return false;
    }
  }
  return true;
}

}  // namespace evolab
