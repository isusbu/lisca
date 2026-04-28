# Linux Static Code Analysis (LisCa)

LisCa is a C++ command-line tool built with LLVM/Clang LibTooling. It searches a compilation database for a function by name and reports:

1. File location.
2. Starting line.
3. Ending line.
4. The signature line.

The project is designed to work well on Linux kernel source trees once you have a `compile_commands.json` database, but it also includes a toy example so you can validate the workflow on a small codebase first.

## Requirements

LisCa needs a C++17 compiler and the LLVM/Clang development packages that provide LibTooling and the CMake package files.

## Linux Build

This is the primary workflow if you want to run LisCa on Linux kernel source code.

On Ubuntu or Debian, install the build requirements first:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build clang llvm-dev libclang-dev bear
```

If your distro splits LLVM by version, you may also have packages such as `llvm-18-dev` and `libclang-18-dev`. Install the matching version for both LLVM and Clang.

Find the LLVM and Clang CMake package directories:

```bash
llvm-config --cmakedir
llvm-config --prefix
```

On Debian and Ubuntu, they are often under one of these paths:

```bash
/usr/lib/llvm-*/lib/cmake/llvm
/usr/lib/llvm-*/lib/cmake/clang
```

If you want to search directly:

```bash
find /usr/lib /usr/local -name LLVMConfig.cmake -o -name ClangConfig.cmake 2>/dev/null
```

Once you know the paths, configure and build LisCa:

```bash
bash ./scripts/configure-build.sh
# or
cmake -S . -B build -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm -DClang_DIR=/usr/lib/llvm-18/lib/cmake/clang
cmake --build build
```

On macOS with Homebrew:

```bash
brew install cmake llvm
```

If you also want the compilation database generator on macOS, install `bear`:

```bash
brew install bear
```

## Find LLVM and Clang Paths

You need the CMake package directories for LLVM and Clang when configuring the build, especially on Linux.

Common commands to find them are:

```bash
llvm-config --cmakedir
clang --print-resource-dir
```

On Homebrew systems, the paths are often:

```bash
brew --prefix llvm
```

Then use the package directories under that prefix, typically:

```bash
$(brew --prefix llvm)/lib/cmake/llvm
$(brew --prefix llvm)/lib/cmake/clang
```

If `clang` is installed elsewhere, you can also search for the package files directly:

```bash
find /usr /opt -name LLVMConfig.cmake -o -name ClangConfig.cmake 2>/dev/null
```

## Build

You need a local LLVM/Clang development installation with CMake package files available.

You can configure and build automatically with the helper script:

```bash
bash ./scripts/configure-build.sh
```

It tries `llvm-config`, then Homebrew, then common install locations to find `LLVM_DIR` and `Clang_DIR`.

```bash
cmake -S . -B build -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm -DClang_DIR=/path/to/clang/lib/cmake/clang
cmake --build build
```

Typical packages on Ubuntu or Debian are `cmake`, `ninja-build`, `clang`, `llvm-dev`, and `libclang-dev`.

## Run on the toy example

Use the toy source in [examples/toy_sample.cpp](examples/toy_sample.cpp) as the first validation target. Generate `compile_commands.json` with the build above, then run:

```bash
./build/lisca --compile-commands-dir build --input examples/toy_sample.cpp --function add
```

Expected output includes the file path, the start and end lines of the function, and the signature line.

You can also try the class method in the same file:

```bash
./build/lisca --compile-commands-dir build --input examples/toy_sample.cpp --function subtract
```

## Run on Linux kernel source

The easiest workflow is to build the kernel with a compilation database and then run LisCa against that database.

### 1. Get the kernel source

```bash
git clone https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
cd linux
```

### 2. Configure the kernel

Start from a clean config that matches your machine or use a default one:

```bash
make defconfig
```

If you only want to analyze a subtree like `fs/ext4`, you do not need to build the whole kernel. A focused module build is faster and usually enough for function lookup.

### 3. Generate a compilation database

Use `bear` to capture the kernel build commands:

```bash
bear -- make -j"$(nproc)" fs/ext4/
```

For a module-focused workflow, this is usually enough:

```bash
bear -- make -j"$(nproc)" M=fs/ext4 modules
```

If you want the whole project, capture a full build instead:

```bash
bear -- make -j"$(nproc)"
```

The important part is that `compile_commands.json` ends up in the directory you pass to `--compile-commands-dir`.

### 4. Build LisCa

Return to the LisCa project and build it once the LLVM and Clang CMake paths are known:

```bash
cmake -S . -B build -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm -DClang_DIR=/path/to/clang/lib/cmake/clang
cmake --build build
```

### 5. Run LisCa on the kernel tree

For a function in `fs/ext4`, point LisCa at the kernel source tree or the specific subtree:

```bash
./build/lisca \
 --compile-commands-dir /path/to/linux \
 --input /path/to/linux/fs/ext4 \
 --function ext4_file_read_iter
```

If your build database was generated from the whole tree, you can also search the full kernel source:

```bash
./build/lisca \
 --compile-commands-dir /path/to/linux \
 --input /path/to/linux \
 --function schedule
```

### 6. Pick functions to test

Good first targets are small, well-known functions such as `ext4_file_read_iter`, `ext4_file_write_iter`, `vfs_read`, or `schedule`. If you get no matches, check that the function is actually compiled in the build database you captured.

If you use `intercept-build` or another compilation-capture tool instead of `bear`, make sure the generated `compile_commands.json` matches the same kernel tree you pass to `--input`.
