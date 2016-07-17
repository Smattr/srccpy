set(LLVM_LINK_COMPONENTS Support)

add_clang_executable(srccpy main.cpp)

target_link_libraries(srccpy clangAST clangASTMatchers clangBasic clangFrontend clangTooling)
