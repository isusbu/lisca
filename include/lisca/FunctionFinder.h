#pragma once

#include "lisca/FunctionInfo.h"

#include <filesystem>
#include <string>
#include <vector>

namespace lisca {

class FunctionFinder {
public:
  std::vector<FunctionInfo> run(const std::filesystem::path &compileCommandsDir,
                                const std::filesystem::path &inputPath,
                                const std::string &functionName) const;
};

} // namespace lisca
