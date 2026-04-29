#include "lisca/FunctionFinder.h"
#include "detail/ASTHelpers.h"
#include "detail/PathUtils.h"

#include <clang/Frontend/Utils.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <string>
#include <vector>

namespace lisca {

std::vector<FunctionInfo> FunctionFinder::run(const std::filesystem::path &compileCommandsDir,
                                              const std::filesystem::path &inputPath,
                                              const std::string &functionName) const {
  std::string error;
  const auto normalizedDir = detail::normalizePath(compileCommandsDir);
  auto database = clang::tooling::CompilationDatabase::loadFromDirectory(
      normalizedDir.string(), error);
  if (!database) return {};

  const auto resolvedInput = detail::resolveInputRoot(normalizedDir, inputPath);
  auto sourceFiles = detail::collectTranslationUnits(*database, resolvedInput);
  if (sourceFiles.empty()) return {};

  for (const std::string &file : sourceFiles) {
    llvm::outs() << "Processing " << file << '\n';
  }

  std::vector<FunctionInfo> results;
  detail::FunctionActionFactory factory(functionName, results);
  clang::tooling::ClangTool tool(*database, sourceFiles);

  auto diagnosticConsumer = std::make_unique<clang::IgnoringDiagConsumer>();
  tool.setDiagnosticConsumer(diagnosticConsumer.release());
  tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
      "--gcc-toolchain=/usr", clang::tooling::ArgumentInsertPosition::BEGIN));

  tool.run(&factory);

  std::sort(results.begin(), results.end(), [](const FunctionInfo &a, const FunctionInfo &b) {
    if (a.filePath != b.filePath)   return a.filePath < b.filePath;
    if (a.startLine != b.startLine) return a.startLine < b.startLine;
    return a.endLine < b.endLine;
  });

  return results;
}

} // namespace lisca
