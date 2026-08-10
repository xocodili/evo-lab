#include "app/CliArgs.hpp"

#include "sim/BarrenWorld.hpp"

#include <iostream>

namespace evolab {

int runHeadlessSmoke(const CliArgs& args) {
  BarrenWorld world(args.seed, args.resolution);

  const WetnessStats startStats = world.wetnessStats();
  if (startStats.wetCells == 0 || startStats.dryCells == 0) {
    std::cerr << "smoke fail: world lacks both land and water\n";
    return 1;
  }

  const float checksum0 = world.heightChecksum();
  int waterLevelChanges = 0;
  float prevWater = world.waterLevel();

  for (int frame = 0; frame < args.frames; ++frame) {
    world.tick();
    const float water = world.waterLevel();
    if (water != prevWater) {
      ++waterLevelChanges;
      prevWater = water;
    }
  }

  if (waterLevelChanges == 0) {
    std::cerr << "smoke fail: tide did not change water level over " << args.frames << " frames\n";
    return 1;
  }

  const WetnessStats endStats = world.wetnessStats();

  BarrenWorld world2(args.seed, args.resolution);
  if (world2.heightChecksum() != checksum0) {
    std::cerr << "smoke fail: heightmap not deterministic for seed " << args.seed << '\n';
    return 1;
  }

  std::cout << "smoke ok: seed=" << args.seed << " frames=" << args.frames << " resolution="
            << args.resolution << " wet=" << endStats.wetCells << " dry=" << endStats.dryCells
            << " checksum=" << checksum0 << '\n';
  return 0;
}

}  // namespace evolab
