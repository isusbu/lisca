cmake_minimum_required(VERSION 3.20)

project(lisca LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ZSTD QUIET)
if(TARGET ZSTD::ZSTD AND NOT TARGET zstd::libzstd_shared)
  add_library(zstd::libzstd_shared ALIAS ZSTD::ZSTD)
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(lisca
  src/FunctionFinder.cpp
  src/main.cpp
)

target_include_directories(lisca PRIVATE include)

llvm_map_components_to_libnames(llvm_libs Support)

target_link_libraries(lisca PRIVATE
  clangAST
  clangBasic
  clangFrontend
  clangLex
  clangTooling
  clangToolingCore
  clangParse
  clangSema
  ${llvm_libs}
)

add_executable(lisca_toy examples/toy_sample.cpp)
