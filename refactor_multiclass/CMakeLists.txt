cmake_minimum_required(VERSION 3.10)
project(find_labels)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})
llvm_map_components_to_libnames(llvm_libs x86asmparser bitreader support mc option profiledata)

add_executable(find_labels multiclass.cpp)
target_include_directories(find_labels PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(find_labels PRIVATE
   clangFrontend
  clangSerialization
  clangDriver
  clangTooling
  clangParse
  clangSema
  clangAnalysis
  clangRewriteFrontend
  clangRewrite
  clangEdit
  clangAST
  clangLex
  clangBasic
  clangASTMatchers
    ${llvm_libs})
set_target_properties(find_labels PROPERTIES COMPILE_FLAGS -fno-rtti)
target_compile_options(find_labels PRIVATE -g)
