#include "lisca/FunctionFinder.h"

#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
  std::string compileCommandsDir;
  std::string inputPath;
  std::string functionName;
};

void printUsage(const char *programName) {
  llvm::outs() << "Usage: " << programName
               << " --compile-commands-dir <dir> --input <path> --function <name>\n";
}

enum class ParseStatus {
  Success,
  Help,
  Error,
};

ParseStatus parseArguments(int argc, char **argv, Options &options) {
  bool encounteredError = false;

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];

    auto requireValue = [&](const char *flag) -> std::string {
      if (index + 1 >= argc) {
        llvm::errs() << "Missing value for " << flag << '\n';
        encounteredError = true;
        return {};
      }
      ++index;
      return argv[index];
    };

    if (argument == "--compile-commands-dir") {
      options.compileCommandsDir = requireValue("--compile-commands-dir");
      if (options.compileCommandsDir.empty()) {
        return ParseStatus::Error;
      }
    } else if (argument == "--input") {
      options.inputPath = requireValue("--input");
      if (options.inputPath.empty()) {
        return ParseStatus::Error;
      }
    } else if (argument == "--function") {
      options.functionName = requireValue("--function");
      if (options.functionName.empty()) {
        return ParseStatus::Error;
      }
    } else if (argument == "--help" || argument == "-h") {
      printUsage(argv[0]);
      return ParseStatus::Help;
    } else {
      llvm::errs() << "Unknown argument: " << argument << '\n';
      return ParseStatus::Error;
    }
  }

  if (encounteredError) {
    return ParseStatus::Error;
  }

  if (options.compileCommandsDir.empty() || options.inputPath.empty() || options.functionName.empty()) {
    llvm::errs() << "Missing required arguments.\n";
    return ParseStatus::Error;
  }

  return ParseStatus::Success;
}

} // namespace

int main(int argc, char **argv) {
  Options options;
  const ParseStatus status = parseArguments(argc, argv, options);
  if (status == ParseStatus::Help) {
    return 0;
  }
  if (status != ParseStatus::Success) {
    printUsage(argv[0]);
    return 1;
  }

  const fs::path compileCommandsDirPath = fs::path(options.compileCommandsDir);
  const fs::path inputPathValue = fs::path(options.inputPath);

  lisca::FunctionFinder finder;
  const std::vector<lisca::FunctionInfo> results =
      finder.run(compileCommandsDirPath, inputPathValue, options.functionName);

  if (results.empty()) {
    llvm::outs() << "No matches found for '" << options.functionName << "'.\n";
    return 1;
  }

  llvm::outs() << "Found " << results.size() << " match(es) for '" << options.functionName
               << "':\n";

  for (const lisca::FunctionInfo &info : results) {
    llvm::outs() << info.filePath << ':' << info.startLine << '-' << info.endLine << '\n';
    llvm::outs() << "  File: " << info.filePath << '\n';
    llvm::outs() << "  LOC: " << (info.endLine - info.startLine + 1) << " lines\n";
    llvm::outs() << "  Function: " << info.functionName << '\n';
    llvm::outs() << "  Signature: " << info.signature << '\n';
    llvm::outs() << '\n';
  }

  return 0;
}
