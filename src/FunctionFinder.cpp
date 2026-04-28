#include "lisca/FunctionFinder.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <set>
#include <utility>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

namespace lisca {
namespace {

namespace fs = std::filesystem;

bool isSourceLike(const fs::path &path) {
  const std::string extension = path.extension().string();
  return extension == ".c" || extension == ".cc" || extension == ".cpp" ||
         extension == ".cxx" || extension == ".h" || extension == ".hh" ||
         extension == ".hpp" || extension == ".hxx";
}

fs::path normalizePath(const fs::path &path) {
  std::error_code errorCode;
  fs::path absolute = fs::absolute(path, errorCode);
  if (errorCode) {
    return path;
  }
  fs::path normalized = fs::weakly_canonical(absolute, errorCode);
  if (errorCode) {
    return absolute.lexically_normal();
  }
  return normalized;
}

fs::path resolveFilePath(const fs::path &directory, const std::string &filename) {
  fs::path filePath(filename);
  if (filePath.is_absolute()) {
    return normalizePath(filePath);
  }
  return normalizePath(directory / filePath);
}

bool startsWithPath(const fs::path &value, const fs::path &prefix) {
  std::error_code errorCode;
  fs::path valueNormalized = fs::weakly_canonical(value, errorCode);
  if (errorCode) {
    valueNormalized = value.lexically_normal();
  }

  fs::path prefixNormalized = fs::weakly_canonical(prefix, errorCode);
  if (errorCode) {
    prefixNormalized = prefix.lexically_normal();
  }

  auto valueText = valueNormalized.lexically_normal().generic_string();
  auto prefixText = prefixNormalized.lexically_normal().generic_string();

  if (prefixText.empty()) {
    return true;
  }

  if (valueText.size() < prefixText.size()) {
    return false;
  }

  if (valueText.compare(0, prefixText.size(), prefixText) != 0) {
    return false;
  }

  if (valueText.size() == prefixText.size()) {
    return true;
  }

  return valueText[prefixText.size()] == '/';
}

std::string getLineText(const SourceManager &sourceManager, SourceLocation location) {
  const FileID fileId = sourceManager.getFileID(location);
  bool invalid = false;
  llvm::StringRef buffer = sourceManager.getBufferData(fileId, &invalid);
  if (invalid) {
    return {};
  }

  const unsigned offset = sourceManager.getFileOffset(location);
  const size_t lineStart = buffer.rfind('\n', offset);
  const size_t begin = lineStart == llvm::StringRef::npos ? 0 : lineStart + 1;
  const size_t lineEnd = buffer.find('\n', offset);
  const size_t length = lineEnd == llvm::StringRef::npos ? buffer.size() - begin : lineEnd - begin;
  return buffer.substr(begin, length).str();
}

class FunctionMatchCallback : public MatchFinder::MatchCallback {
public:
  explicit FunctionMatchCallback(std::vector<FunctionInfo> &results)
      : results_(results) {}

  void run(const MatchFinder::MatchResult &result) override {
    const auto *functionDecl = result.Nodes.getNodeAs<FunctionDecl>("function");
    if (functionDecl == nullptr || result.SourceManager == nullptr) {
      return;
    }

    const SourceManager &sourceManager = *result.SourceManager;
    const SourceLocation beginLocation = sourceManager.getSpellingLoc(functionDecl->getBeginLoc());
    const SourceLocation endLocation = sourceManager.getSpellingLoc(
        Lexer::getLocForEndOfToken(functionDecl->getEndLoc(), 0, sourceManager,
                                   result.Context->getLangOpts()));

    const PresumedLoc beginPresumed = sourceManager.getPresumedLoc(beginLocation);
    const PresumedLoc endPresumed = sourceManager.getPresumedLoc(endLocation);
    if (beginPresumed.isInvalid() || endPresumed.isInvalid()) {
      return;
    }

    FunctionInfo info;
    info.filePath = beginPresumed.getFilename();
    info.functionName = functionDecl->getQualifiedNameAsString();
    info.startLine = beginPresumed.getLine();
    info.endLine = endPresumed.getLine();
    info.signature = getLineText(sourceManager, beginLocation);
    results_.push_back(std::move(info));
  }

private:
  std::vector<FunctionInfo> &results_;
};

class FunctionConsumer : public ASTConsumer {
public:
  FunctionConsumer(std::string functionName, std::vector<FunctionInfo> &results)
      : callback_(results) {
    finder_.addMatcher(functionDecl(isDefinition(), hasName(functionName))
                           .bind("function"),
                       &callback_);
  }

  void HandleTranslationUnit(ASTContext &context) override {
    finder_.matchAST(context);
  }

private:
  FunctionMatchCallback callback_;
  MatchFinder finder_;
};

class FunctionAction : public ASTFrontendAction {
public:
  FunctionAction(std::string functionName, std::vector<FunctionInfo> &results)
      : functionName_(std::move(functionName)), results_(results) {}

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &, StringRef) override {
    return std::make_unique<FunctionConsumer>(functionName_, results_);
  }

private:
  std::string functionName_;
  std::vector<FunctionInfo> &results_;
};

class FunctionActionFactory : public FrontendActionFactory {
public:
  FunctionActionFactory(std::string functionName, std::vector<FunctionInfo> &results)
      : functionName_(std::move(functionName)), results_(results) {}

  std::unique_ptr<FrontendAction> create() override {
    return std::make_unique<FunctionAction>(functionName_, results_);
  }

private:
  std::string functionName_;
  std::vector<FunctionInfo> &results_;
};

std::vector<std::string> collectTranslationUnits(const CompilationDatabase &database,
                                                 const fs::path &inputPath) {
  std::vector<std::string> sourceFiles;
  std::set<std::string> seenFiles;

  const fs::path normalizedInput = normalizePath(inputPath);
  const bool inputIsDirectory = fs::is_directory(normalizedInput);

  for (const CompileCommand &command : database.getAllCompileCommands()) {
    const fs::path filePath = resolveFilePath(command.Directory, command.Filename);
    if (!isSourceLike(filePath)) {
      continue;
    }

    if (inputIsDirectory) {
      if (!startsWithPath(filePath, normalizedInput)) {
        continue;
      }
    } else if (normalizePath(filePath) != normalizedInput) {
      continue;
    }

    const std::string canonicalKey = normalizePath(filePath).generic_string();
    if (seenFiles.insert(canonicalKey).second) {
      sourceFiles.push_back(canonicalKey);
    }
  }

  return sourceFiles;
}

fs::path resolveInputRoot(const fs::path &compileCommandsDir, const fs::path &inputPath) {
  if (inputPath.is_absolute()) {
    return inputPath;
  }

  return compileCommandsDir / inputPath;
}

} // namespace

std::vector<FunctionInfo> FunctionFinder::run(const fs::path &compileCommandsDir,
                                              const fs::path &inputPath,
                                              const std::string &functionName) const {
  std::string errorMessage;
  const fs::path normalizedCompileCommandsDir = normalizePath(compileCommandsDir);
  std::unique_ptr<CompilationDatabase> database =
      CompilationDatabase::loadFromDirectory(normalizedCompileCommandsDir.string(), errorMessage);
  if (!database) {
    llvm::errs() << "Failed to load compilation database: " << errorMessage << '\n';
    return {};
  }

  const fs::path resolvedInputPath = resolveInputRoot(normalizedCompileCommandsDir, inputPath);

  std::vector<std::string> sourceFiles = collectTranslationUnits(*database, resolvedInputPath);
  if (sourceFiles.empty()) {
    llvm::errs() << "No analyzable translation units found under " << resolvedInputPath << '\n';
    return {};
  }

  std::vector<FunctionInfo> results;
  FunctionActionFactory factory(functionName, results);
  ClangTool tool(*database, sourceFiles);

  tool.appendArgumentsAdjuster(
      getInsertArgumentAdjuster("--gcc-toolchain=/usr", ArgumentInsertPosition::BEGIN));

  if (const int code = tool.run(&factory); code != 0) {
    llvm::errs() << "ClangTool exited with code " << code << '\n';
  }

  std::sort(results.begin(), results.end(), [](const FunctionInfo &lhs, const FunctionInfo &rhs) {
    if (lhs.filePath != rhs.filePath) {
      return lhs.filePath < rhs.filePath;
    }
    if (lhs.startLine != rhs.startLine) {
      return lhs.startLine < rhs.startLine;
    }
    return lhs.endLine < rhs.endLine;
  });

  return results;
}

} // namespace lisca
