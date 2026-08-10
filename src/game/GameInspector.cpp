#include "game/GameInspector.hpp"

namespace evolab::game {

namespace {

const char* kInspectSizeTemplate =
    "Organism #99999\n"
    "Type: twin mouth (2 M, 2 axons)\n"
    "Nodes: 99  Bone: 99  Heading: 999 deg\n"
    "Body: 999999999 bytes (99.99 d)  Node stores: 999999999\n"
    "Axon M1→M2 feed:999 believe:999 last:0xFF recv:yes\n"
    "Axon M2→M1 feed:999 believe:999 last:0xFF recv:yes\n"
    "Land-adjacent: yes  tick 9999999999  alive";

}  // namespace

bool GameInspector::init(const engine::gfx::UiFont& font) {
  return panel_.init(font, kInspectSizeTemplate, engine::gfx::TextOverlayAnchor::BottomLeft);
}

void GameInspector::shutdown() { panel_.shutdown(); }

void GameInspector::draw(const std::string& text, int viewportW, int viewportH) {
  panel_.draw(text, viewportW, viewportH);
}

}  // namespace evolab::game
