cmake_minimum_required(VERSION 3.21)

project(psr_database
    VERSION 1.0.0
    DESCRIPTION "Cross-platform database library with multi-language bindings"
    LANGUAGES CXX C
)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(PSR_BUILD_SHARED "Build shared library" ON)
option(PSR_BUILD_TESTS "Build test suite" ON)
option(PSR_BUILD_EXAMPLES "Build examples" OFF)

# Binding options
option(PSR_BUILD_PYTHON_BINDING "Build Python binding" OFF)
option(PSR_BUILD_JULIA_BINDING "Build Julia binding" OFF)
option(PSR_BUILD_DART_BINDING "Build Dart-compatible C API" OFF)
option(PSR_BUILD_C_API "Build C API wrapper" OFF)

# Auto-enable C API if any FFI binding is requested
if(PSR_BUILD_DART_BINDING OR PSR_BUILD_JULIA_BINDING)
    set(PSR_BUILD_C_API ON CACHE BOOL "" FORCE)
endif()

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)
include(Platform)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Core library
add_subdirectory(src)

# C API wrapper
if(PSR_BUILD_C_API)
    add_subdirectory(src_c)
endif()

# Language bindings
if(PSR_BUILD_PYTHON_BINDING)
    add_subdirectory(bindings)
endif()

# Tests
if(PSR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(PSR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Format target
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/include/*.h
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src_c/*.h
        ${CMAKE_SOURCE_DIR}/src_c/*.cpp
        ${CMAKE_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_SOURCE_DIR}/bindings/python/src/*.cpp
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting code with clang-format"
    )
endif()
