#pragma once

#include "app/CliArgs.hpp"

namespace evolab {

int runHeadlessSmoke(const CliArgs& args);
int runHeadlessDebugSession(const CliArgs& args);

}  // namespace evolab
