# Srccpy - a C rewriter

This repository contains a tool for automating C code refactoring. Sample usage:

```bash
# Rename all instances of the function "foo" to "bar"
srccpy -rename=foo=bar -output=my_new_code.c my_code.c --
```

It currently has the following abilities:

  * Rename function definitions

## Building

Srccpy is written as a Clang/LLVM tool. This means it expects to be built within the Clang/LLVM
source tree. To build it, you need to follow the libtooling instructions, reproduced here for
completeness:

```bash
# Clone necessary Clang/LLVM pieces
git clone http://llvm.org/git/llvm.git
cd llvm/tools
git clone http://llvm.org/git/clang.git
cd clang/tools
git clone http://llvm.org/git/clang-tools-extra.git extra

# Clone this repository into the right directory
cd extra
git clone https://github.com/Smattr/srccpy

# Tell the Clang/LLVM build system about this new tool
echo 'add_subdirectory(srccpy)' >>CMakeLists.txt

# Build the tool
cd ../../../../..
mkdir build
cd build
cmake -G Ninja ../llvm -DCMAKE_BUILD_TYPE=Release
ninja srccpy
```

It requires a lot of memory to link Clang and libclang. On a low memory system you may want to
append these flags to CMake:

  * `-DCMAKE_EXE_LINKER_FLAGS="-Wl,--no-keep-memory -Wl,--reduce-memory-overheads"`
  * `-DCMAKE_SHARED_LINKER_FLAGS="-Wl,--no-keep-memory -Wl,--reduce-memory-overheads"`

and these flags to Ninja:

  * `-j 1`

This will slow your compilation a lot but may be the only way to get it to succeed if you have less
than 16GB of RAM.

## Legal

Everything in this repository is in the public domain. Use it anyway you wish.
