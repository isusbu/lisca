#include "lisca/FunctionFinder.h"

#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
  std::string compileCommandsDir;
  std::string inputPath;
  std::string functionName;
  std::string prefix;
  std::string outputFile;
};

void printUsage(const char *programName) {
  llvm::outs() << "Usage: " << programName
               << " --compile-commands-dir <dir> --input <path>"
               << " (--function <name> | --prefix <prefix>)"
               << " [--output <file>]\n";
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
      if (options.compileCommandsDir.empty()) return ParseStatus::Error;
    } else if (argument == "--input") {
      options.inputPath = requireValue("--input");
      if (options.inputPath.empty()) return ParseStatus::Error;
    } else if (argument == "--function") {
      options.functionName = requireValue("--function");
      if (options.functionName.empty()) return ParseStatus::Error;
    } else if (argument == "--prefix") {
      options.prefix = requireValue("--prefix");
      if (options.prefix.empty()) return ParseStatus::Error;
    } else if (argument == "--output") {
      options.outputFile = requireValue("--output");
      if (options.outputFile.empty()) return ParseStatus::Error;
    } else if (argument == "--help" || argument == "-h") {
      printUsage(argv[0]);
      return ParseStatus::Help;
    } else {
      llvm::errs() << "Unknown argument: " << argument << '\n';
      return ParseStatus::Error;
    }
  }

  if (encounteredError) return ParseStatus::Error;

  if (options.compileCommandsDir.empty() || options.inputPath.empty()) {
    llvm::errs() << "Missing required arguments.\n";
    return ParseStatus::Error;
  }
  if (options.functionName.empty() && options.prefix.empty()) {
    llvm::errs() << "One of --function or --prefix is required.\n";
    return ParseStatus::Error;
  }
  if (!options.functionName.empty() && !options.prefix.empty()) {
    llvm::errs() << "--function and --prefix are mutually exclusive.\n";
    return ParseStatus::Error;
  }

  return ParseStatus::Success;
}

void printResults(llvm::raw_ostream &out, const std::string &query,
                  const std::vector<lisca::FunctionInfo> &results, bool isPrefix) {
  if (results.empty()) {
    out << "No matches found for '" << query << "'.\n";
    return;
  }

  if (isPrefix) {
    // Per-function details
    out << "Found " << results.size() << " function(s) with prefix '" << query << "':\n\n";
    for (const lisca::FunctionInfo &info : results) {
      out << info.filePath << ':' << info.startLine << '-' << info.endLine << '\n';
      out << "  Function:  " << info.functionName << '\n';
      out << "  Signature: " << info.signature << '\n';
      out << "  LOC:       " << (info.endLine - info.startLine + 1) << " lines\n";
      out << '\n';
    }

    // Aggregate stats
    unsigned totalLoc = 0;
    std::map<std::string, unsigned> locPerFile;
    std::map<std::string, unsigned> countPerFile;
    for (const lisca::FunctionInfo &info : results) {
      const unsigned loc = info.endLine - info.startLine + 1;
      totalLoc += loc;
      locPerFile[info.filePath] += loc;
      countPerFile[info.filePath]++;
    }

    out << "=== Stats for prefix '" << query << "' ===\n";
    out << "  Total functions : " << results.size() << '\n';
    out << "  Total LOC       : " << totalLoc << '\n';
    out << "  Files touched   : " << countPerFile.size() << '\n';
    out << '\n';
    out << "  Per-file breakdown:\n";
    for (const auto &[file, count] : countPerFile) {
      out << "    " << file << '\n';
      out << "      Functions : " << count << '\n';
      out << "      LOC       : " << locPerFile.at(file) << '\n';
    }
  } else {
    out << "Found " << results.size() << " match(es) for '" << query << "':\n";
    for (const lisca::FunctionInfo &info : results) {
      out << info.filePath << ':' << info.startLine << '-' << info.endLine << '\n';
      out << "  File:      " << info.filePath << '\n';
      out << "  LOC:       " << (info.endLine - info.startLine + 1) << " lines\n";
      out << "  Function:  " << info.functionName << '\n';
      out << "  Signature: " << info.signature << '\n';
      out << '\n';
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  Options options;
  const ParseStatus status = parseArguments(argc, argv, options);
  if (status == ParseStatus::Help) return 0;
  if (status != ParseStatus::Success) {
    printUsage(argv[0]);
    return 1;
  }

  const bool isPrefix = !options.prefix.empty();
  const std::string query = isPrefix ? options.prefix : options.functionName;
  const lisca::MatchMode mode = isPrefix ? lisca::MatchMode::Prefix : lisca::MatchMode::Exact;

  lisca::FunctionFinder finder;
  const std::vector<lisca::FunctionInfo> results =
      finder.run(fs::path(options.compileCommandsDir), fs::path(options.inputPath), query, mode);

  // Always print to stdout
  printResults(llvm::outs(), query, results, isPrefix);

  // Optionally export to file
  if (!options.outputFile.empty()) {
    std::ofstream file(options.outputFile);
    if (!file) {
      llvm::errs() << "Failed to open output file: " << options.outputFile << '\n';
      return 1;
    }
    std::string buffer;
    llvm::raw_string_ostream sout(buffer);
    printResults(sout, query, results, isPrefix);
    file << sout.str();
    llvm::outs() << "Output written to " << options.outputFile << '\n';
  }

  return results.empty() ? 1 : 0;
}
