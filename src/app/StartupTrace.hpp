#pragma once

#include <fstream>
#include <string>

namespace evolab {

class StartupTrace {
public:
  StartupTrace() = default;
  void open(const std::string& path);
  void step(const char* label);

private:
  std::ofstream out_;
  std::string path_;
};

}  // namespace evolab
