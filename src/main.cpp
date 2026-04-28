#include "lisca/FunctionFinder.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static llvm::cl::OptionCategory LiscaCategory("LisCa options");
static llvm::cl::opt<std::string> CompileCommandsDir(
    "compile-commands-dir", llvm::cl::desc("Path to the directory containing compile_commands.json"),
    llvm::cl::value_desc("directory"), llvm::cl::Required, llvm::cl::cat(LiscaCategory));
static llvm::cl::opt<std::string> InputPath(
    "input", llvm::cl::desc("File or directory to analyze"), llvm::cl::value_desc("path"),
    llvm::cl::Required, llvm::cl::cat(LiscaCategory));
static llvm::cl::opt<std::string> FunctionName(
    "function", llvm::cl::desc("Function name to search for"), llvm::cl::value_desc("name"),
    llvm::cl::Required, llvm::cl::cat(LiscaCategory));

int main(int argc, char **argv) {
  llvm::cl::HideUnrelatedOptions(LiscaCategory);
  llvm::cl::ParseCommandLineOptions(argc, argv, "LisCa - function locator\n");

  const fs::path compileCommandsDirPath = fs::path(CompileCommandsDir.getValue());
  const fs::path inputPathValue = fs::path(InputPath.getValue());

  lisca::FunctionFinder finder;
  const std::vector<lisca::FunctionInfo> results =
      finder.run(compileCommandsDirPath, inputPathValue, FunctionName.getValue());

  if (results.empty()) {
    llvm::outs() << "No matches found for '" << FunctionName.getValue() << "'.\n";
    return 1;
  }

  llvm::outs() << "Found " << results.size() << " match(es) for '" << FunctionName.getValue()
               << "':\n";

  for (const lisca::FunctionInfo &info : results) {
    llvm::outs() << info.filePath << ':' << info.startLine << '-' << info.endLine << '\n';
    llvm::outs() << "  Function: " << info.functionName << '\n';
    llvm::outs() << "  Signature: " << info.signature << '\n';
  }

  return 0;
}
