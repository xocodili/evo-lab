#include "sim/CampTopology.hpp"

#include <algorithm>
#include <string>

namespace evolab {

namespace {

char campNeuronLetter(NeuronType type) {
  switch (type) {
    case NeuronType::Perceptor:
      return 'P';
    case NeuronType::Mouth:
      return 'M';
    case NeuronType::Computer:
      return 'C';
    case NeuronType::Actuator:
      return 'A';
    default:
      return '?';
  }
}

}  // namespace

bool organismHasCampNeuronFloor(const Organism& organism) {
  if (!organism.alive) {
    return false;
  }
  bool hasPerceptor = false;
  bool hasMouth = false;
  bool hasComputer = false;
  bool hasActuator = false;
  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    switch (node.neuron) {
      case NeuronType::Perceptor:
        hasPerceptor = true;
        break;
      case NeuronType::Mouth:
        hasMouth = true;
        break;
      case NeuronType::Computer:
        hasComputer = true;
        break;
      case NeuronType::Actuator:
        hasActuator = true;
        break;
      default:
        break;
    }
  }
  return hasPerceptor && hasMouth && hasComputer && hasActuator;
}

bool organismHasCampDevelopmentalAxons(const Organism& organism) {
  for (const auto& edge : kCampDevelopmentalAxons) {
    if (organism.findNeuralAxon(edge.first, edge.second) == nullptr) {
      return false;
    }
  }
  return true;
}

bool isCampDevelopmentalAxonEdge(std::uint32_t srcId, std::uint32_t dstId) {
  for (const auto& edge : kCampDevelopmentalAxons) {
    if (edge.first == srcId && edge.second == dstId) {
      return true;
    }
  }
  return false;
}

bool isCampTorpedoChainLinkEdge(std::uint32_t parentId, std::uint32_t childId) {
  for (const auto& edge : kCampTorpedoChainLinks) {
    if (edge.first == parentId && edge.second == childId) {
      return true;
    }
  }
  return false;
}

bool organismHasCampHubArms(const Organism& organism) {
  if (organism.rootNodeId == 0) {
    return false;
  }
  const SkeletonNode* hub = organism.findNode(organism.computerNodeId != 0 ? organism.computerNodeId
                                                                           : organism.rootNodeId);
  if (hub == nullptr || hub->neuron != NeuronType::Computer) {
    return false;
  }
  bool hasPerceptorArm = false;
  bool hasMouthArm = false;
  bool hasActuatorArm = false;
  for (const SkeletonLink& link : organism.links) {
    if (link.parentNodeId != hub->id) {
      continue;
    }
    const SkeletonNode* child = organism.findNode(link.childNodeId);
    if (child == nullptr || !child->alive) {
      continue;
    }
    switch (child->neuron) {
      case NeuronType::Perceptor:
        hasPerceptorArm = true;
        break;
      case NeuronType::Mouth:
        hasMouthArm = true;
        break;
      case NeuronType::Actuator:
        hasActuatorArm = true;
        break;
      default:
        break;
    }
  }
  return hasPerceptorArm && hasMouthArm && hasActuatorArm;
}

bool organismHasCampTorpedoSkeleton(const Organism& organism) {
  if (organism.nodes.size() < 4 || organism.links.size() < kCampTorpedoChainSegmentCount) {
    return false;
  }
  int matched = 0;
  for (const auto& edge : kCampTorpedoChainLinks) {
    for (const SkeletonLink& link : organism.links) {
      if (link.parentNodeId == edge.first && link.childNodeId == edge.second) {
        ++matched;
        break;
      }
    }
  }
  return matched >= static_cast<int>(kCampTorpedoChainSegmentCount);
}

bool organismHasCampTorpedoChain(const Organism& organism) {
  if (!organismHasCampNeuronFloor(organism)) {
    return false;
  }
  return organismHasCampTorpedoSkeleton(organism);
}

bool organismUsesCampNeuronPhases(const Organism& organism) {
  if (!organism.alive || !organism.hasNeuralAxons()) {
    return false;
  }
  if (organismHasCampTopology(organism)) {
    return true;
  }
  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    if (node.neuron == NeuronType::Perceptor || node.neuron == NeuronType::Mouth ||
        node.neuron == NeuronType::Computer || node.neuron == NeuronType::Actuator) {
      return true;
    }
  }
  return false;
}

bool organismUsesCampSkeletonVisual(const Organism& organism) {
  if (!organism.alive || organism.nodes.size() < 4) {
    return false;
  }
  if (organismHasCampTorpedoSkeleton(organism)) {
    int muscleSegments = 0;
    for (const SkeletonLink& link : organism.links) {
      if (!link.muscleBundle) {
        continue;
      }
      for (const auto& edge : kCampTorpedoChainLinks) {
        if (link.parentNodeId == edge.first && link.childNodeId == edge.second) {
          ++muscleSegments;
          break;
        }
      }
    }
    return muscleSegments >= static_cast<int>(kCampTorpedoChainSegmentCount);
  }
  const SkeletonNode* hub = organism.findNode(organism.computerNodeId != 0 ? organism.computerNodeId
                                                                           : organism.rootNodeId);
  if (hub == nullptr || hub->neuron != NeuronType::Computer) {
    return false;
  }
  int muscleArms = 0;
  for (const SkeletonLink& link : organism.links) {
    if (!link.muscleBundle || link.parentNodeId != hub->id) {
      continue;
    }
    if (link.childNodeId == kCampPerceptorId || link.childNodeId == kCampMouthId ||
        link.childNodeId == kCampActuatorId) {
      ++muscleArms;
    }
  }
  return muscleArms >= 3;
}

std::string campGenotypeLabel(const Organism& organism) {
  std::string label;
  label.reserve(organism.nodes.size());
  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    label.push_back(campNeuronLetter(node.neuron));
  }
  return label;
}

std::string campTorpedoMorphologyLabel(const Organism& organism) {
  if (!organismHasCampTorpedoSkeleton(organism)) {
    return campGenotypeLabel(organism);
  }

  std::string label;
  label.reserve(kCampTorpedoChainSegmentCount + 1);
  std::uint32_t nodeId = kCampRootNodeId;
  for (std::size_t step = 0; step <= kCampTorpedoChainSegmentCount; ++step) {
    const SkeletonNode* node = organism.findNode(nodeId);
    if (node == nullptr || !node->alive) {
      return campGenotypeLabel(organism);
    }
    label.push_back(campNeuronLetter(node->neuron));
    std::uint32_t childId = 0;
    for (const auto& edge : kCampTorpedoChainLinks) {
      if (edge.first == nodeId) {
        childId = edge.second;
        break;
      }
    }
    if (childId == 0) {
      break;
    }
    nodeId = childId;
  }

  std::reverse(label.begin(), label.end());
  return label;
}

std::string campDisplayTypeLabel(const Organism& organism) {
  if (organismHasCampTorpedoChain(organism)) {
    return campTorpedoMorphologyLabel(organism);
  }
  return campGenotypeLabel(organism);
}

bool organismHasCampTopology(const Organism& organism) {
  if (!organism.hasPerceptorNeurons() || !organism.hasMouthNeurons() ||
      !organism.hasActuatorNeurons()) {
    return false;
  }
  int computerCount = 0;
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == NeuronType::Computer) {
      ++computerCount;
    }
  }
  if (computerCount != 1 || organism.perceptorCount() != 1 || organism.mouthCount() != 1 ||
      organism.actuatorCount() != 1 || organism.nodes.size() != 4) {
    return false;
  }

  for (const auto& edge : kCampDevelopmentalAxons) {
    if (organism.findNeuralAxon(edge.first, edge.second) == nullptr) {
      return false;
    }
  }
  return true;
}

}  // namespace evolab
