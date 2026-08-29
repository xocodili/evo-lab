#include "sim/CampTopology.hpp"

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
