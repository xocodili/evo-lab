#pragma once



#include "sim/Energon.hpp"

#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"



#include <cstdint>

#include <random>

#include <string>

#include <vector>



namespace evolab {



class BarrenWorld;

class EnergonField;



enum class NeuronType : std::uint8_t { None = 0, Mouth, Perceptor, Computer, Actuator };



// Joint / attachment site. World pose is filled each tick by kinematics.

struct SkeletonNode {

  std::uint32_t id = 0;

  NeuronType neuron = NeuronType::None;

  float worldX = 0.0f;

  float worldY = 0.0f;

  float worldZ = 0.0f;

  std::vector<std::uint8_t> store;

  bool ateThisTick = false;

  std::uint8_t lastEmittedByte = 0;

};



// Coupled mechanical bone (kinematic). Energy on this edge is optional (eta may be 0).

struct SkeletonLink {

  std::uint32_t parentNodeId = 0;

  std::uint32_t childNodeId = 0;

  float restLength = 0.0f;

  float jointAngle = 0.0f;

  float energyEta = 0.88f;

};



struct ColonyAxon {

  std::uint32_t targetOrganismId = 0;

  float trust = 1.0f;

};



class Organism {

public:

  std::uint32_t id = 0;

  std::uint64_t createdAtTick = 0;

  std::uint32_t rootNodeId = 0;

  std::vector<SkeletonNode> nodes;

  std::vector<SkeletonLink> links;

  std::vector<NeuralAxon> neuralAxons;

  std::vector<ColonyAxon> colonyAxons;

  std::vector<std::uint8_t> bodyStorage;

  bool landAdjacent = false;

  bool alive = true;

  float heading = 0.0f;



  SkeletonNode* findNode(std::uint32_t nodeId);

  const SkeletonNode* findNode(std::uint32_t nodeId) const;

  NeuralAxon* findNeuralAxon(std::uint32_t srcNodeId, std::uint32_t dstNodeId);

  const NeuralAxon* findNeuralAxon(std::uint32_t srcNodeId, std::uint32_t dstNodeId) const;



  float rootWorldX() const;

  float rootWorldY() const;

  float rootWorldZ() const;



  void updateKinematics(const BarrenWorld& world, float cellSize, float heightScale);

  void advectRoot(const BarrenWorld& world, const EnergonField& energon, float cellSize,

                  float heightScale, float halfExtent);

  void metabolise(const BarrenWorld& world, float cellSize, float heightScale);

  void feed(EnergonField& field, float cellSize);

  void transferEnergy(EnergonField& field, float cellSize);

  void signal(std::uint64_t simTick);

  void transferColony();



  int mouthCount() const;

  bool hasMouthNeurons() const;

  bool hasNeuralAxons() const;

  // Single spawn hook: developmental axon trust + skeleton jitter (once per parameter).
  void finalizeSpawn(std::mt19937& rng);

  // Remove neural edges whose trust has degraded fully to zero (structural pruning).
  void pruneNeuralAxons();

  bool allLocalStoresEmpty() const;



  std::string architectureLabel() const;

  std::string hoverSummary() const;

};



Organism makeUndifferentiatedOrganism(std::uint32_t id, float wx, float wz, float wy,

                                      std::size_t storageBytes, std::uint64_t createdAtTick);



Organism makeStarMouthOrganism(std::uint32_t id, float wx, float wz, float wy,

                               std::size_t storageBytes, std::uint64_t createdAtTick,

                               int mouthCount, float boneLength);



Organism makeTwoMouthOrganism(std::uint32_t id, float wx, float wz, float wy,

                              std::size_t storageBytes, std::uint64_t createdAtTick,

                              float boneLength,
                              std::uint16_t trustFeedM1ToM2 = kNeuralAxonDefaultTrust,
                              std::uint16_t trustFeedM2ToM1 = kNeuralAxonDefaultTrust);



bool organismLandAdjacent(const BarrenWorld& world, float wx, float wz, float cellSize);



}  // namespace evolab

