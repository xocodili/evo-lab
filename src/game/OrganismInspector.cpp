#include "game/OrganismInspector.hpp"

#include "sim/CellConstants.hpp"
#include "sim/WorldConstants.hpp"

#include <cstdio>

namespace evolab::game {

namespace {

int trustDisplayPercent(std::uint16_t trust) {
  return static_cast<int>((static_cast<unsigned>(trust) * 100u + kTrustBaseline / 2u) /
                          kTrustBaseline);
}

const char* signalTagLabel(std::uint8_t tag) {
  switch (tag) {
    case kSignalTagIAte:
      return "I_ATE";
    case kSignalTagIHunger:
      return "I_HUNGER";
    case kSignalTagIActuate:
      return "I_ACTUATE";
    case kSignalTagISenseFood:
      return "SENSE_FOOD";
    case kSignalTagISenseOrganism:
      return "SENSE_ORGANISM";
    case kSignalTagISenseBlock:
      return "SENSE_BLOCK";
    case kMouthSignalTagShipping:
      return "SHIPPING";
    default:
      return "—";
  }
}

const SkeletonNode* findPerceptorNode(const Organism& organism) {
  for (const SkeletonNode& node : organism.nodes) {
    if (node.neuron == NeuronType::Perceptor) {
      return &node;
    }
  }
  return nullptr;
}

const SkeletonNode* findMouthNode(const Organism& organism) {
  for (const SkeletonNode& node : organism.nodes) {
    if (node.neuron == NeuronType::Mouth) {
      return &node;
    }
  }
  return nullptr;
}

const SkeletonNode* findActuatorNode(const Organism& organism) {
  for (const SkeletonNode& node : organism.nodes) {
    if (node.neuron == NeuronType::Actuator) {
      return &node;
    }
  }
  return nullptr;
}

float daysOfEnergon(std::size_t bytes) {
  return static_cast<float>(bytes) / static_cast<float>(kTicksPerStemCellDay);
}

}  // namespace

std::string formatOrganismArchitectureLabel(const Organism& organism, std::uint64_t simTick) {
  char buffer[2048];
  const float daysRemaining = daysOfEnergon(organism.bodyStorage.size());

  if (!organism.hasMouthNeurons() && !organism.hasActuatorNeurons()) {
    std::snprintf(buffer, sizeof(buffer),
                  "StemCell #%u\n"
                  "Type: undifferentiated\n"
                  "Nodes: 1  Links: 0\n"
                  "Storage: %zu bytes (%.2f d)\n"
                  "Land-adjacent: %s\n"
                  "Created: tick %llu\n"
                  "Status: %s",
                  organism.id, organism.bodyStorage.size(), daysRemaining,
                  organism.landAdjacent ? "yes" : "no",
                  static_cast<unsigned long long>(organism.createdAtTick),
                  organism.alive ? "alive" : "dead");
    return buffer;
  }

  if (organism.isPmaNom()) {
    const SkeletonNode* perceptorNode = findPerceptorNode(organism);
    const SkeletonNode* mouthNode = findMouthNode(organism);
    const SkeletonNode* actuatorNode = findActuatorNode(organism);
    const NeuralAxon* pToM = organism.findNeuralAxon(1, 2);
    const NeuralAxon* pToA = organism.findNeuralAxon(1, 3);
    const NeuralAxon* mToA = organism.findNeuralAxon(2, 3);
    const NeuralAxon* aToM = organism.findNeuralAxon(3, 2);
    char pToMRecv[12] = "—";
    char pToARecv[12] = "—";
    if (pToM != nullptr && pToM->lastReceived.valid) {
      std::snprintf(pToMRecv, sizeof(pToMRecv), "0x%02X", pToM->lastReceived.byte);
    }
    if (pToA != nullptr && pToA->lastReceived.valid) {
      std::snprintf(pToARecv, sizeof(pToARecv), "0x%02X", pToA->lastReceived.byte);
    }
    std::snprintf(buffer, sizeof(buffer),
                  "Nom #%u\n"
                  "Type: P-M-A Nom\n"
                  "Nodes: 3  Links: 2  Axons: 4\n"
                  "Heading: %.0f deg\n"
                  "Energon (tick %llu):\n"
                  "  P [sense]:  %zu B  %s  scan: %s (%u B)\n"
                  "  M [mouth]:  %zu B  %s  ate: %s\n"
                  "  A [motor]:  %zu B  %s\n"
                  "Perception (last tick):\n"
                  "  tag: %s (0x%02X)  bearing: %+.0f deg  range: %.0f%%\n"
                  "Signals (last tick):\n"
                  "  P->M: %s (%s)  P->A: %s (%s)\n"
                  "  M->A: %s  A->M: %s\n"
                  "  stroke: %s (%u B)  inhibit: %s\n"
                  "Land-adjacent: %s  %s",
                  organism.id, organism.heading * 180.0f / 3.14159265f,
                  static_cast<unsigned long long>(simTick != 0 ? simTick : organism.createdAtTick),
                  perceptorNode != nullptr ? perceptorNode->store.size() : 0,
                  perceptorNode != nullptr && perceptorNode->alive ? "alive" : "dead",
                  organism.lastPerceptScanPaid ? "paid" : "skipped", organism.lastPerceptBytesPaid,
                  mouthNode != nullptr ? mouthNode->store.size() : 0,
                  mouthNode != nullptr && mouthNode->alive ? "alive" : "dead",
                  mouthNode != nullptr && mouthNode->alive && mouthNode->ateThisTick ? "yes" : "no",
                  actuatorNode != nullptr ? actuatorNode->store.size() : 0,
                  actuatorNode != nullptr && actuatorNode->alive ? "alive" : "dead",
                  organism.lastPerceptTag != 0 ? signalTagLabel(organism.lastPerceptTag) : "—",
                  organism.lastPerceptTag, organism.lastPerceptBearing * 180.0f / 3.14159265f,
                  organism.lastPerceptRange * 100.0f, pToMRecv,
                  pToM != nullptr && pToM->lastReceived.valid
                      ? signalTagLabel(pToM->lastReceived.byte)
                      : "—",
                  pToARecv,
                  pToA != nullptr && pToA->lastReceived.valid
                      ? signalTagLabel(pToA->lastReceived.byte)
                      : "—",
                  mToA != nullptr && mToA->lastReceived.valid ? "active" : "—",
                  aToM != nullptr && aToM->lastReceived.valid ? "active" : "—",
                  organism.lastStrokePaid ? "paid" : "skipped", organism.lastStrokeBytesPaid,
                  organism.lastActuatorInhibited ? "yes (I_ATE)" : "no",
                  organism.landAdjacent ? "yes" : "no", organism.alive ? "alive" : "dead");
    return buffer;
  }

  if (organism.hasActuatorNeurons() && !organism.hasMouthNeurons()) {
    const float strokeEfficiency = organism.lastIntendedThrust > 0.0f
                                       ? organism.lastMechanicalThrust / organism.lastIntendedThrust
                                       : 0.0f;
    const float displacementEfficiency = organism.lastIntendedThrust > 0.0f
                                             ? organism.lastDisplacement / organism.lastIntendedThrust
                                             : 0.0f;
    std::snprintf(buffer, sizeof(buffer),
                  "Nom #%u\n"
                  "Type: actuator (1 A, no mouth)\n"
                  "Storage: %zu bytes (%.2f d)\n"
                  "Heading: %.0f deg\n"
                  "Proprioception (last tick):\n"
                  "  displacement: %.4f\n"
                  "  gross thrust intent: %.4f\n"
                  "  mechanical thrust: %.4f (eta %.0f%%)\n"
                  "  stroke paid: %u B (%s)\n"
                  "  of stroke -> heat: %.2f B (not extra charge)\n"
                  "  stroke efficiency: %.2f  path/tide: %.2f\n"
                  "  tide delta: %+.5f\n"
                  "  crawl burn/tick: %u B (basal %u + stroke %u)\n"
                  "  in water: %s\n"
                  "  tumbled: %s\n"
                  "Land-adjacent: %s\n"
                  "Created: tick %llu\n"
                  "Status: %s",
                  organism.id, organism.bodyStorage.size(), daysRemaining,
                  organism.heading * 180.0f / 3.14159265f, organism.lastDisplacement,
                  organism.lastIntendedThrust, organism.lastMechanicalThrust,
                  kActuatorTranslationEta * 100.0f, organism.lastStrokeBytesPaid,
                  organism.lastStrokePaid ? "this tick" : "skipped",
                  organism.lastTranslationEntropyLoss, strokeEfficiency, displacementEfficiency,
                  organism.lastTideDelta, kActuatorCrawlCostPerTick, kStemCellBasalCostPerTick,
                  kActuatorStrokeCostPerTick, organism.lastInWater ? "yes" : "no — stranded",
                  organism.lastTumbled ? "yes" : "no", organism.landAdjacent ? "yes" : "no",
                  static_cast<unsigned long long>(organism.createdAtTick),
                  organism.alive ? "alive" : "dead");
    return buffer;
  }

  std::size_t localBytes = 0;
  for (const SkeletonNode& node : organism.nodes) {
    localBytes += node.store.size();
  }

  if (organism.hasNeuralAxons() && organism.mouthCount() == 2 && organism.nodes.size() == 2) {
    const NeuralAxon* axon12 = organism.findNeuralAxon(1, 2);
    const NeuralAxon* axon21 = organism.findNeuralAxon(2, 1);
    char recv12Byte[8] = "—";
    if (axon12 != nullptr && axon12->lastReceived.valid) {
      std::snprintf(recv12Byte, sizeof(recv12Byte), "0x%02X", axon12->lastReceived.byte);
    }
    char recv21Byte[8] = "—";
    if (axon21 != nullptr && axon21->lastReceived.valid) {
      std::snprintf(recv21Byte, sizeof(recv21Byte), "0x%02X", axon21->lastReceived.byte);
    }
    std::snprintf(buffer, sizeof(buffer),
                  "Organism #%u\n"
                  "Type: twin mouth (2 M, 2 axons)\n"
                  "Nodes: 2  Bone: 1  Heading: %.0f deg\n"
                  "Body: %zu bytes (%.2f d)  Node stores: %zu\n"
                  "Axon M1->M2 feed:%d%% believe:%d%% last:0x%02X recv:%s\n"
                  "Axon M2->M1 feed:%d%% believe:%d%% last:0x%02X recv:%s\n"
                  "Land-adjacent: %s  tick %llu  %s",
                  organism.id, organism.heading * 180.0f / 3.14159265f, organism.bodyStorage.size(),
                  daysRemaining, localBytes,
                  axon12 != nullptr ? trustDisplayPercent(axon12->trustFeed) : 0,
                  axon12 != nullptr ? trustDisplayPercent(axon12->trustBelieve) : 0,
                  axon12 != nullptr ? axon12->lastSentByte : 0, recv12Byte,
                  axon21 != nullptr ? trustDisplayPercent(axon21->trustFeed) : 0,
                  axon21 != nullptr ? trustDisplayPercent(axon21->trustBelieve) : 0,
                  axon21 != nullptr ? axon21->lastSentByte : 0, recv21Byte,
                  organism.landAdjacent ? "yes" : "no",
                  static_cast<unsigned long long>(organism.createdAtTick),
                  organism.alive ? "alive" : "dead");
    return buffer;
  }

  std::snprintf(buffer, sizeof(buffer),
                "Organism #%u\n"
                "Type: kinetic mouth (%d M)\n"
                "Nodes: %zu  Links: %zu  Heading: %.0f deg\n"
                "Body storage: %zu bytes (%.2f d)\n"
                "Node stores: %zu bytes\n"
                "Land-adjacent: %s\n"
                "Created: tick %llu\n"
                "Status: %s",
                organism.id, organism.mouthCount(), organism.nodes.size(), organism.links.size(),
                organism.heading * 180.0f / 3.14159265f, organism.bodyStorage.size(),
                daysRemaining, localBytes, organism.landAdjacent ? "yes" : "no",
                static_cast<unsigned long long>(organism.createdAtTick),
                organism.alive ? "alive" : "dead");
  return buffer;
}

std::string formatOrganismHoverSummary(const Organism& organism) {
  char buffer[160];
  if (organism.isPmaNom()) {
    const SkeletonNode* perceptorNode = findPerceptorNode(organism);
    const SkeletonNode* mouthNode = findMouthNode(organism);
    const SkeletonNode* actuatorNode = findActuatorNode(organism);
    std::snprintf(buffer, sizeof(buffer),
                  "Hover: Nom #%u P-M-A P %zu M %zu A %zu sense %s",
                  organism.id,
                  perceptorNode != nullptr ? perceptorNode->store.size() : 0,
                  mouthNode != nullptr ? mouthNode->store.size() : 0,
                  actuatorNode != nullptr ? actuatorNode->store.size() : 0,
                  organism.lastPerceptScanPaid ? signalTagLabel(organism.lastPerceptTag) : "—");
  } else if (organism.hasActuatorNeurons() && !organism.hasMouthNeurons()) {
    std::snprintf(buffer, sizeof(buffer), "Hover: Nom #%u actuator (d=%.3f, stroke %s)",
                  organism.id, organism.lastDisplacement,
                  organism.lastStrokePaid ? "paid" : "skipped");
  } else if (!organism.hasMouthNeurons()) {
    std::snprintf(buffer, sizeof(buffer), "Hover: StemCell #%u (undifferentiated)", organism.id);
  } else if (organism.hasNeuralAxons() && organism.mouthCount() == 2) {
    std::snprintf(buffer, sizeof(buffer), "Hover: Organism #%u twin mouth (2 axons)", organism.id);
  } else {
    std::snprintf(buffer, sizeof(buffer), "Hover: Organism #%u kinetic mouth (%d M, %zu links)",
                  organism.id, organism.mouthCount(), organism.links.size());
  }
  return buffer;
}

}  // namespace evolab::game
