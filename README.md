# Linux Static Code Analysis (LisCa)

LisCa is a C++ command-line tool built with LLVM/Clang LibTooling. It searches a compilation database for a function by name and reports:

1. File location.
2. Starting line.
3. Ending line.
4. The signature line.

The project is designed to work well on Linux kernel source trees once you have a `compile_commands.json` database, but it also includes a toy example so you can validate the workflow on a small codebase first.

## Build

You need a local LLVM/Clang development installation with CMake package files available.

```bash
cmake -S . -B build -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm -DClang_DIR=/path/to/clang/lib/cmake/clang
cmake --build build
```

## Try on the toy example

Use the toy source in [examples/toy_sample.cpp](examples/toy_sample.cpp) as the first validation target. Generate `compile_commands.json` with the build above, then run:

```bash
./build/lisca --compile-commands-dir build --input examples/toy_sample.cpp --function add
```

Expected output includes the file path, the start and end lines of the function, and the signature line.

## Use on Linux kernel source

Generate a compilation database for the kernel source tree, then point LisCa at the kernel build directory and source tree:

```bash
./build/lisca --compile-commands-dir /path/to/kernel/build --input /path/to/linux --function schedule
```

If you use `bear`, `intercept-build`, or another tool to generate `compile_commands.json`, make sure the database reflects the same source tree you pass to `--input`.
