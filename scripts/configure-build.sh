#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$repo_root/build}"

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "error: missing required command: $1" >&2
    exit 1
  fi
}

find_clang_dir() {
  local llvm_prefix="$1"
  local candidate="$llvm_prefix/lib/cmake/clang"
  if [[ -d "$candidate" ]]; then
    printf '%s\n' "$candidate"
    return 0
  fi

  candidate="$(find /usr /opt -type d -path '*/lib/cmake/clang' 2>/dev/null | head -n 1 || true)"
  if [[ -n "$candidate" ]]; then
    printf '%s\n' "$candidate"
    return 0
  fi

  return 1
}

detect_from_llvm_config() {
  if ! command -v llvm-config >/dev/null 2>&1; then
    return 1
  fi

  local llvm_dir
  local llvm_prefix
  llvm_dir="$(llvm-config --cmakedir)"
  llvm_prefix="$(llvm-config --prefix)"

  if [[ -d "$llvm_dir" ]]; then
    printf '%s\n%s\n' "$llvm_dir" "$(find_clang_dir "$llvm_prefix")"
    return 0
  fi

  return 1
}

detect_from_brew() {
  if ! command -v brew >/dev/null 2>&1; then
    return 1
  fi

  local llvm_prefix
  llvm_prefix="$(brew --prefix llvm 2>/dev/null || true)"
  if [[ -z "$llvm_prefix" || ! -d "$llvm_prefix" ]]; then
    return 1
  fi

  local llvm_dir="$llvm_prefix/lib/cmake/llvm"
  local clang_dir="$llvm_prefix/lib/cmake/clang"
  if [[ -d "$llvm_dir" && -d "$clang_dir" ]]; then
    printf '%s\n%s\n' "$llvm_dir" "$clang_dir"
    return 0
  fi

  return 1
}

require_command cmake

llvm_dir=""
clang_dir=""

if mapfile -t detected < <(detect_from_llvm_config); then
  if [[ ${#detected[@]} -eq 2 ]]; then
    llvm_dir="${detected[0]}"
    clang_dir="${detected[1]}"
  fi
fi

if [[ -z "$llvm_dir" || -z "$clang_dir" ]]; then
  if mapfile -t detected < <(detect_from_brew); then
    if [[ ${#detected[@]} -eq 2 ]]; then
      llvm_dir="${detected[0]}"
      clang_dir="${detected[1]}"
    fi
  fi
fi

if [[ -z "$llvm_dir" || -z "$clang_dir" ]]; then
  echo "error: unable to detect LLVM_DIR and Clang_DIR" >&2
  echo "hint: install llvm/clang development packages or pass paths manually to cmake" >&2
  exit 1
fi

echo "LLVM_DIR=$llvm_dir"
echo "Clang_DIR=$clang_dir"

cmake -S "$repo_root" -B "$build_dir" -DLLVM_DIR="$llvm_dir" -DClang_DIR="$clang_dir"
cmake --build "$build_dir"
