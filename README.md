# psr_database

A cross-platform C++17 SQLite wrapper library with a C API for FFI integration.

## Features

- Modern C++17 API with RAII resource management
- Cross-platform support (Windows, Linux, macOS)
- SQLite embedded via CMake FetchContent
- C API for FFI integration with other languages

## Building

### Prerequisites

- CMake 3.21 or higher
- C++17 compatible compiler:
  - GCC 8+ / Clang 7+ (Linux/macOS)
  - MSVC 2019+ or MinGW (Windows)

### Basic Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build -C Release --output-on-failure
```

### Using CMake Presets

```bash
# Development build with tests
cmake --preset dev
cmake --build build/dev

# Release build
cmake --preset release
cmake --build build/release
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `PSR_BUILD_SHARED` | ON | Build shared library |
| `PSR_BUILD_TESTS` | ON | Build test suite |
| `PSR_BUILD_C_API` | OFF | Build C API wrapper |

## Usage

### C++

```cpp
#include <psr_database/psr_database.h>

psr::Database db(":memory:");
db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
db.execute("INSERT INTO users (name) VALUES (?)", {psr::Value{"Alice"}});

auto result = db.execute("SELECT * FROM users");
for (const auto& row : result) {
    std::cout << row.get_string(1).value() << std::endl;
}
```

### C API

```c
#include <psr_database_c.h>

psr_database_t* db = psr_database_open(":memory:");
psr_database_execute(db, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");

psr_result_t* result = psr_database_execute(db, "SELECT * FROM users");
// ... iterate over results
psr_result_free(result);
psr_database_close(db);
```

## Testing

```bash
# Build with tests enabled
cmake -B build -DPSR_BUILD_TESTS=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

## Project Structure

```
psr_database/
├── cmake/                  # CMake modules
├── include/psr_database/   # Public C++ headers
├── src/                    # Core library implementation
├── src_c/                  # C API wrapper
└── tests/                  # C++ test suite (GoogleTest)
```

## License

See LICENSE file for details.
