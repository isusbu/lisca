#pragma once

#include "lisca/FunctionInfo.h"
#include "detail/PathUtils.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringRef.h>

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace lisca::detail {

// Returns the full source text of the line containing loc.
inline std::string getLineText(const clang::SourceManager &sm, clang::SourceLocation loc) {
  const clang::FileID fileId = sm.getFileID(loc);
  bool invalid = false;
  llvm::StringRef buffer = sm.getBufferData(fileId, &invalid);
  if (invalid) return {};

  const unsigned offset   = sm.getFileOffset(loc);
  const size_t lineStart  = buffer.rfind('\n', offset);
  const size_t begin      = (lineStart == llvm::StringRef::npos) ? 0 : lineStart + 1;
  const size_t lineEnd    = buffer.find('\n', offset);
  const size_t length     = (lineEnd == llvm::StringRef::npos) ? buffer.size() - begin
                                                                : lineEnd - begin;
  return buffer.substr(begin, length).str();
}

// Collects FunctionInfo entries for each matched function definition.
class FunctionMatchCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  explicit FunctionMatchCallback(std::vector<FunctionInfo> &results) : results_(results) {}

  void run(const clang::ast_matchers::MatchFinder::MatchResult &result) override {
    const auto *decl = result.Nodes.getNodeAs<clang::FunctionDecl>("function");
    if (!decl || !result.SourceManager) return;

    const clang::SourceManager &sm = *result.SourceManager;
    const clang::SourceLocation beginLoc = sm.getSpellingLoc(decl->getBeginLoc());
    const clang::SourceLocation endLoc   = sm.getSpellingLoc(
        clang::Lexer::getLocForEndOfToken(decl->getEndLoc(), 0, sm,
                                          result.Context->getLangOpts()));

    const clang::PresumedLoc beginPresumed = sm.getPresumedLoc(beginLoc);
    const clang::PresumedLoc endPresumed   = sm.getPresumedLoc(endLoc);
    if (beginPresumed.isInvalid() || endPresumed.isInvalid()) return;

    FunctionInfo info;
    info.filePath     = beginPresumed.getFilename();
    info.functionName = decl->getQualifiedNameAsString();
    info.startLine    = beginPresumed.getLine();
    info.endLine      = endPresumed.getLine();
    info.signature    = getLineText(sm, beginLoc);
    results_.push_back(std::move(info));
  }

private:
  std::vector<FunctionInfo> &results_;
};

// Registers an AST matcher for a named function definition and drives matching.
class FunctionConsumer : public clang::ASTConsumer {
public:
  FunctionConsumer(std::string functionName, std::vector<FunctionInfo> &results)
      : callback_(results) {
    finder_.addMatcher(
        clang::ast_matchers::functionDecl(
            clang::ast_matchers::isDefinition(),
            clang::ast_matchers::hasName(functionName))
            .bind("function"),
        &callback_);
  }

  void HandleTranslationUnit(clang::ASTContext &ctx) override {
    finder_.matchAST(ctx);
  }

private:
  FunctionMatchCallback callback_;
  clang::ast_matchers::MatchFinder finder_;
};

// Frontend action that creates a FunctionConsumer for each translation unit.
class FunctionAction : public clang::ASTFrontendAction {
public:
  FunctionAction(std::string functionName, std::vector<FunctionInfo> &results)
      : functionName_(std::move(functionName)), results_(results) {}

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &, llvm::StringRef) override {
    return std::make_unique<FunctionConsumer>(functionName_, results_);
  }

private:
  std::string functionName_;
  std::vector<FunctionInfo> &results_;
};

// Factory that produces a FunctionAction for each source file in the tool run.
class FunctionActionFactory : public clang::tooling::FrontendActionFactory {
public:
  FunctionActionFactory(std::string functionName, std::vector<FunctionInfo> &results)
      : functionName_(std::move(functionName)), results_(results) {}

  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<FunctionAction>(functionName_, results_);
  }

private:
  std::string functionName_;
  std::vector<FunctionInfo> &results_;
};

// Returns unique, normalized paths from database that fall under inputPath
// (which may be a single file or a directory).
inline std::vector<std::string> collectTranslationUnits(
    const clang::tooling::CompilationDatabase &database,
    const std::filesystem::path &inputPath) {
  namespace fs = std::filesystem;

  std::vector<std::string> sourceFiles;
  std::set<std::string> seen;

  const fs::path normalizedInput = normalizePath(inputPath);
  const bool inputIsDir = fs::is_directory(normalizedInput);

  for (const clang::tooling::CompileCommand &cmd : database.getAllCompileCommands()) {
    const fs::path filePath = resolveFilePath(cmd.Directory, cmd.Filename);
    if (!isSourceLike(filePath)) continue;

    if (inputIsDir) {
      if (!startsWithPath(filePath, normalizedInput)) continue;
    } else if (normalizePath(filePath) != normalizedInput) {
      continue;
    }

    const std::string key = normalizePath(filePath).generic_string();
    if (seen.insert(key).second) sourceFiles.push_back(key);
  }

  return sourceFiles;
}

} // namespace lisca::detail
