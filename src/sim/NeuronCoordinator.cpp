#include "sim/NeuronCoordinator.hpp"

#include "sim/CellConstants.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/OrganismNeuron.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

namespace {

float nodeFuelUnit(const Organism& organism, const SkeletonNode& node) {
  const std::vector<std::uint8_t>* pool = neuronFuelPool(const_cast<Organism&>(organism), node);
  if (pool == nullptr) {
    return 0.0f;
  }
  const std::size_t maxBytes = nodeStoreNominalCap(organism, node);
  return clamp01(static_cast<float>(pool->size()) /
                 static_cast<float>(std::max<std::size_t>(maxBytes, 1u)));
}

float nodeSenseDrive(const Organism& organism, const SkeletonNode& node) {
  switch (node.neuron) {
    case NeuronType::Mouth:
      return clamp01(node.mouthTasteSalience + std::max(0.0f, node.mouthTasteGradient) * 0.35f);
    case NeuronType::Perceptor:
      return clamp01(node.focusSalience * (node.focusLocked ? 1.0f : 0.65f));
    case NeuronType::Actuator:
      return organism.lastStrokePaid ? 1.0f : kCoordinatorBaselineDutyScale;
    case NeuronType::Computer:
      // mini-C reads last tick's organ match; full C updates match later same frame.
      return clamp01(node.lastComputerMatchScore);
    case NeuronType::None:
      return nodeFuelUnit(organism, node);
    default:
      return 0.5f;
  }
}

std::uint8_t encodeCoordinatorObserved(const CoordinatorInteroception& interoception) {
  const float combined =
      clamp01(interoception.fuelUnit * 0.55f + interoception.senseDrive * 0.45f);
  return static_cast<std::uint8_t>(std::lround(combined * static_cast<float>(kNeuronConfidenceMax)));
}

float computeNodeDutyScale(SkeletonNode& node, const Organism& organism,
                           CoordinatorInteroception& outInteroception) {
  outInteroception.fuelUnit = nodeFuelUnit(organism, node);
  outInteroception.senseDrive = nodeSenseDrive(organism, node);
  outInteroception.observedByte = encodeCoordinatorObserved(outInteroception);
  outInteroception.patternMatch =
      neuronConfidenceMatchUnit(outInteroception.observedByte, node.coordinatorRegister[0]);

  outInteroception.excitation = clamp01(outInteroception.patternMatch * 0.4f +
                                        outInteroception.senseDrive * kCoordinatorExcitationGain +
                                        outInteroception.fuelUnit * 0.25f);

  outInteroception.delta = outInteroception.excitation - node.coordinatorPriorExcitation;
  node.coordinatorPriorExcitation = outInteroception.excitation;
  node.coordinatorLastExcitation = outInteroception.excitation;
  node.coordinatorLastDelta = outInteroception.delta;

  const float hubUnit =
      clamp01(static_cast<float>(computerHubFuelBytes(organism)) /
              static_cast<float>(std::max<std::size_t>(hubStoreCapBytes(organism), 1u)));
  const float starvationBoost = (1.0f - outInteroception.fuelUnit) * kCoordinatorStarvationBoost;
  const float satiationBrake = hubUnit * kCoordinatorSatiationBrake;

  float duty = kCoordinatorBaselineDutyScale + outInteroception.excitation * 0.35f +
               outInteroception.delta * kCoordinatorDeltaGain + starvationBoost - satiationBrake;
  duty = std::clamp(duty, kCoordinatorMinDutyScale, kCoordinatorMaxDutyScale);
  node.coordinatorDutyScale = duty;
  return duty;
}

void syncOrganismCoordinatorTelemetry(Organism& organism) {
  float minDuty = 1.0f;
  float maxDuty = 0.0f;
  float rootDuty = 1.0f;
  bool any = false;

  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    any = true;
    minDuty = std::min(minDuty, node.coordinatorDutyScale);
    maxDuty = std::max(maxDuty, node.coordinatorDutyScale);
    if (node.id == organism.rootNodeId) {
      rootDuty = node.coordinatorDutyScale;
    }
  }

  if (!any) {
    organism.coordinatorDutyScale = 1.0f;
    organism.coordinatorMinNodeDuty = 1.0f;
    organism.coordinatorMaxNodeDuty = 1.0f;
    return;
  }
  organism.coordinatorDutyScale = rootDuty;
  organism.coordinatorMinNodeDuty = minDuty;
  organism.coordinatorMaxNodeDuty = maxDuty;
}

}  // namespace

CoordinatorInteroception gatherCoordinatorInteroception(const Organism& organism,
                                                        const SkeletonNode& node) {
  CoordinatorInteroception interoception;
  interoception.fuelUnit = nodeFuelUnit(organism, node);
  interoception.senseDrive = nodeSenseDrive(organism, node);
  interoception.observedByte = encodeCoordinatorObserved(interoception);
  interoception.patternMatch =
      neuronConfidenceMatchUnit(interoception.observedByte, node.coordinatorRegister[0]);
  interoception.excitation =
      clamp01(interoception.patternMatch * 0.4f +
              interoception.senseDrive * kCoordinatorExcitationGain + interoception.fuelUnit * 0.25f);
  interoception.delta = interoception.excitation - node.coordinatorPriorExcitation;
  return interoception;
}

void initCoordinatorNodeRegister(SkeletonNode& node) {
  node.coordinatorRegister = {kNeuronConfidenceNeutral, 0u, 0u, 0u};
  node.coordinatorDutyScale = 1.0f;
  node.coordinatorPriorExcitation = 0.0f;
  node.coordinatorLastExcitation = 0.0f;
  node.coordinatorLastDelta = 0.0f;
}

void tickCoordinatorPhase(Organism& organism, std::uint64_t simTick) {
  (void)simTick;
  if (!organism.alive) {
    return;
  }

  for (SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    CoordinatorInteroception scratch;
    computeNodeDutyScale(node, organism, scratch);
  }

  syncOrganismCoordinatorTelemetry(organism);
}

float coordinatorDutyScaleForNode(const Organism& organism, std::uint32_t nodeId) {
  const SkeletonNode* node = organism.findNode(nodeId);
  if (node == nullptr || !node->alive) {
    return 1.0f;
  }
  return node->coordinatorDutyScale;
}

float applyMiniCToComputerDispatch(float organDispatchGain, float coordinatorDutyScale) {
  return std::clamp(organDispatchGain * clamp01(coordinatorDutyScale), kComputerMinDispatchGain,
                    1.0f);
}

}  // namespace evolab
