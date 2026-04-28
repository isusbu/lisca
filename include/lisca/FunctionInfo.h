#pragma once

#include <string>

namespace lisca {

struct FunctionInfo {
  std::string filePath;
  std::string functionName;
  std::string signature;
  unsigned startLine = 0;
  unsigned endLine = 0;
};

} // namespace lisca
