#include "app/StartupTrace.hpp"

#include <iostream>

namespace evolab {

void StartupTrace::open(const std::string& path) {
  if (out_) {
    return;
  }
  path_ = path;
  out_.open(path_, std::ios::out | std::ios::trunc);
  if (!out_) {
    std::cerr << "Could not write startup trace to: " << path_ << '\n';
  }
}

void StartupTrace::step(const char* label) {
  if (out_) {
    out_ << label << '\n';
    out_.flush();
  }
}

}  // namespace evolab
