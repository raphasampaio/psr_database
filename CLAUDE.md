# CLAUDE.md

This file provides guidance for Claude Code when working with this repository.

## Project Overview

**psr_database** is a cross-platform C++17 SQLite wrapper library with language bindings for Python, Julia, and Dart.

## Build Commands

```bash
# Configure and build (development)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DPSR_BUILD_TESTS=ON -DPSR_BUILD_C_API=ON
cmake --build build

# Configure and build (release)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run tests
ctest --test-dir build -C Debug --output-on-failure

# Using presets
cmake --preset dev && cmake --build build/dev
cmake --preset release && cmake --build build/release
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `PSR_BUILD_SHARED` | ON | Build shared library (.dll/.so) |
| `PSR_BUILD_TESTS` | ON | Build GoogleTest suite |
| `PSR_BUILD_EXAMPLES` | OFF | Build example programs |
| `PSR_BUILD_C_API` | OFF | Build C API wrapper (auto-enabled for FFI bindings) |
| `PSR_BUILD_PYTHON_BINDING` | OFF | Build Python pybind11 binding |

## Project Structure

```
psr_database/
├── include/psr_database/   # Public C++ headers
│   ├── database.h          # Database class
│   ├── result.h            # Result/Row classes
│   ├── export.h            # DLL export macros
│   └── psr_database.h      # Main include header
├── src/                    # Core library implementation
│   ├── database.cpp
│   └── result.cpp
├── src_c/                  # C API wrapper (extern "C")
│   ├── psr_database_c.h    # C API header
│   └── psr_database_c.cpp
├── bindings/
│   ├── python/             # pybind11 binding
│   ├── julia/PsrDatabase/  # Julia package (ccall)
│   └── dart/               # dart:ffi binding
├── tests/                  # GoogleTest suite
├── examples/               # Usage examples per language
└── cmake/                  # CMake modules
```

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                 Language Bindings                    │
├───────────┬───────────┬───────────┬─────────────────┤
│  Python   │   Julia   │   Dart    │     C/C++       │
│ (pybind11)│  (ccall)  │ (dart:ffi)│    (direct)     │
├───────────┴───────────┴───────────┴─────────────────┤
│                 C API (src_c/)                       │
│            psr_database_c.h - extern "C"             │
├─────────────────────────────────────────────────────┤
│              Core C++ Library (src/)                 │
│         psr::Database, psr::Result, psr::Row         │
├─────────────────────────────────────────────────────┤
│                     SQLite                           │
│             (via CMake FetchContent)                 │
└─────────────────────────────────────────────────────┘
```

## Code Conventions

- **C++ Standard**: C++17
- **Namespace**: `psr`
- **Naming**: snake_case for functions/variables, PascalCase for classes
- **Headers**: Use `#pragma once` or include guards
- **Formatting**: Follow `.clang-format` (LLVM-based, 4-space indent)

## Key Classes

### C++ API (`include/psr_database/`)

- `psr::Database` - SQLite database connection wrapper
  - `execute(sql)` / `execute(sql, params)` - Execute queries
  - `begin_transaction()`, `commit()`, `rollback()` - Transaction control
  - `last_insert_rowid()`, `changes()` - Query metadata

- `psr::Result` - Query result container (iterable)
  - `row_count()`, `column_count()`, `columns()`
  - `operator[]` for row access

- `psr::Row` - Single result row
  - `get_int()`, `get_double()`, `get_string()`, `get_blob()`
  - `is_null()` - Check for NULL values

- `psr::Value` - Variant type: `nullptr_t | int64_t | double | string | vector<uint8_t>`

### C API (`src_c/psr_database_c.h`)

- Opaque handles: `psr_database_t*`, `psr_result_t*`
- Error codes: `PSR_OK`, `PSR_ERROR_*`
- Functions prefixed with `psr_database_*` and `psr_result_*`

## Testing

Tests use GoogleTest. Test files:
- `tests/test_database.cpp` - Database class tests
- `tests/test_result.cpp` - Result/Row class tests
- `tests/test_c_api.cpp` - C API tests (requires `PSR_BUILD_C_API=ON`)

Run specific test:
```bash
./build/bin/psr_database_tests --gtest_filter="DatabaseTest.*"
```

## Adding New Features

1. **Core functionality**: Add to `include/psr_database/` and `src/`
2. **Expose via C API**: Update `src_c/psr_database_c.h` and `.cpp`
3. **Update bindings**: Each binding wraps either C++ directly (Python) or C API (Julia, Dart)
4. **Add tests**: Update `tests/test_*.cpp`

## Dependencies

- **SQLite**: Fetched automatically via CMake FetchContent
- **GoogleTest**: Fetched automatically when `PSR_BUILD_TESTS=ON`
- **pybind11**: Fetched automatically when `PSR_BUILD_PYTHON_BINDING=ON`

## Platform Notes

- **Windows**: Builds with MSVC or MinGW. DLLs output to `build/bin/`
- **Linux**: Builds with GCC/Clang. Shared libs output to `build/lib/`
- **macOS**: Similar to Linux, uses `.dylib` extension
