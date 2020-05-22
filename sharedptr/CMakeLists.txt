cmake_minimum_required(VERSION 3.10)
project(smartptr)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})
llvm_map_components_to_libnames(llvm_libs x86asmparser bitreader support mc option profiledata)

add_executable(smartptrize src/smart_ptrize.cc)
target_include_directories(smartptrize PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(smartptrize PRIVATE
    clangAST
    clangAnalysis
    clangBasic
    clangDriver
    clangFormat
    clangFrontend
    clangFrontendTool
    clangLex
    clangParse
    clangSema
    clangEdit
    clangASTMatchers
    clangRewrite
    clangRewriteFrontend
    clangStaticAnalyzerFrontend
    clangStaticAnalyzerCheckers
    clangStaticAnalyzerCore
    clangSerialization
    clangToolingCore
    clangTooling
    clangToolingInclusions
    LLVM
    ${llvm_libs})
set_target_properties(smartptrize PROPERTIES COMPILE_FLAGS -fno-rtti)
target_compile_options(smartptrize PRIVATE -g)
