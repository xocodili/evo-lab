#include "sim/CampTopology.hpp"

namespace evolab {

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
