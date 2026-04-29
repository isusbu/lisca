#pragma once

#include <filesystem>
#include <string>

namespace lisca::detail {

namespace fs = std::filesystem;

// Returns true if the file extension is a recognised C/C++ source or header type.
inline bool isSourceLike(const fs::path &path) {
  const std::string ext = path.extension().string();
  return ext == ".c"   || ext == ".cc"  || ext == ".cpp" || ext == ".cxx" ||
         ext == ".h"   || ext == ".hh"  || ext == ".hpp" || ext == ".hxx";
}

// Returns an absolute, weakly-canonical path; falls back gracefully on error.
inline fs::path normalizePath(const fs::path &path) {
  std::error_code ec;
  fs::path absolute = fs::absolute(path, ec);
  if (ec) return path;
  fs::path canonical = fs::weakly_canonical(absolute, ec);
  if (ec) return absolute.lexically_normal();
  return canonical;
}

// Resolves filename relative to directory; returns normalized result.
// If filename is already absolute, directory is ignored.
inline fs::path resolveFilePath(const fs::path &directory, const std::string &filename) {
  fs::path filePath(filename);
  if (filePath.is_absolute()) return normalizePath(filePath);
  return normalizePath(directory / filePath);
}

// Returns true when value equals prefix or is nested directly under it.
inline bool startsWithPath(const fs::path &value, const fs::path &prefix) {
  std::error_code ec;

  auto valueNorm = fs::weakly_canonical(value, ec);
  if (ec) valueNorm = value.lexically_normal();

  auto prefixNorm = fs::weakly_canonical(prefix, ec);
  if (ec) prefixNorm = prefix.lexically_normal();

  const auto valueStr  = valueNorm.lexically_normal().generic_string();
  const auto prefixStr = prefixNorm.lexically_normal().generic_string();

  if (prefixStr.empty()) return true;
  if (valueStr.size() < prefixStr.size()) return false;
  if (valueStr.compare(0, prefixStr.size(), prefixStr) != 0) return false;
  if (valueStr.size() == prefixStr.size()) return true;
  return valueStr[prefixStr.size()] == '/';
}

// Makes inputPath absolute using compileCommandsDir as the base when needed.
inline fs::path resolveInputRoot(const fs::path &compileCommandsDir,
                                 const fs::path &inputPath) {
  if (inputPath.is_absolute()) return inputPath;
  return compileCommandsDir / inputPath;
}

} // namespace lisca::detail
