# CLAUDE.md

This file provides guidance for Claude Code when working with this repository.

## Project Overview

**psr_database** is a cross-platform C++17 SQLite wrapper library with a C API for FFI integration.

## Quick Reference

```bash
# Build (release with C API)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPSR_BUILD_C_API=ON
cmake --build build --config Release

# Test
ctest --test-dir build -C Release --output-on-failure

# Install
cmake --install build --prefix /usr/local
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `PSR_BUILD_SHARED` | ON | Build shared library (.dll/.so) |
| `PSR_BUILD_TESTS` | ON | Build GoogleTest suite |
| `PSR_BUILD_C_API` | OFF | Build C API wrapper |

## Project Structure

```
psr_database/
├── include/psr/        # Public headers
│   ├── database.h      # C++ Database class
│   ├── result.h        # C++ Result/Row/Value
│   ├── export.h        # DLL export macros
│   └── c/              # C API headers
│       ├── database.h
│       └── result.h
├── src/                # Implementation
│   ├── database.cpp
│   ├── result.cpp
│   └── c_api.cpp       # C API wrapper
├── tests/
└── cmake/
```

## Architecture

```
┌─────────────────────────────────────────┐
│          C API (src/c_api.cpp)          │
│        include/psr/c/ - extern "C"      │
├─────────────────────────────────────────┤
│        Core C++ Library (src/)          │
│   psr::Database, psr::Result, psr::Row  │
├─────────────────────────────────────────┤
│               SQLite                    │
│       (via CMake FetchContent)          │
└─────────────────────────────────────────┘
```

## Code Conventions

- **C++ Standard**: C++17
- **Namespace**: `psr`
- **Naming**: snake_case for functions/variables, PascalCase for classes

## Key Classes

### C++ API

- `psr::Database` - Connection wrapper (execute, transactions)
- `psr::Result` - Query result container (iterable)
- `psr::Row` - Single row (get_int, get_string, get_blob, etc.)
- `psr::Value` - Variant: `nullptr_t | int64_t | double | string | vector<uint8_t>`

### C API

- Handles: `psr_database_t*`, `psr_result_t*`
- Error codes: `PSR_OK`, `PSR_ERROR_*`
- Functions: `psr_database_*`, `psr_result_*`

## Adding Features

1. Add to `include/psr/` and `src/`
2. Expose via C API in `include/psr/c/` and `src/c_api.cpp`
3. Add tests in `tests/`

## Platform Notes

- **Windows**: DLLs in `build/bin/`
- **Linux/macOS**: Shared libs in `build/lib/`
