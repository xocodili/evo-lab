#include "game/GameInspector.hpp"

namespace evolab::game {

namespace {

const char* kInspectSizeTemplate =
    "Nom #99999\n"
    "Type: CAMP Nom\n"
    "Nodes: 4  Links: 3  Axons: 12\n"
    "Heading: 999 deg\n"
    "Energon (tick 9999999999):\n"
    "  P [sense]:  999999 B  alive  scan: paid (99 B)\n"
    "  M [mouth]:  999999 B  alive  ate: yes\n"
    "  A [motor]:  999999 B  alive\n"
    "Perception (last tick):\n"
    "  tag: SENSE_ORGANISM (0xB2)  bearing: +999 deg  range: 999%\n"
    "Signals (analog 0-7, last tick):\n"
    "  P->M: 7/7 (approach/avoid)  P->A: 7/7 (approach/avoid)\n"
    "  M->P: 7/7 (fuel/satiation)  A->P: 7/7 (flagella activity)\n"
    "  M->A: 7/7 (fuel/satiation)  A->M: 7/7 (flagella activity)\n"
    "  stroke: paid (99 B)  inhibit: yes (M fuel high)\n"
    "Land-adjacent: yes  alive";

}  // namespace

bool GameInspector::init(const engine::gfx::UiFont& font) {
  return panel_.init(font, kInspectSizeTemplate, engine::gfx::TextOverlayAnchor::BottomLeft);
}

void GameInspector::shutdown() { panel_.shutdown(); }

void GameInspector::draw(const std::string& text, int viewportW, int viewportH) {
  panel_.draw(text, viewportW, viewportH);
}

}  // namespace evolab::game
