# psr_database

A cross-platform C++17 SQLite wrapper library with a C API for FFI integration.

## Features

- Modern C++17 API with RAII resource management
- Cross-platform support (Windows, Linux, macOS)
- SQLite embedded via CMake FetchContent
- C API for FFI integration with other languages
- CMake package config for easy integration

## Building

### Prerequisites

- CMake 3.21+
- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+, MinGW)

### Quick Start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPSR_BUILD_C_API=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Install

```bash
cmake --install build --prefix /usr/local
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `PSR_BUILD_SHARED` | ON | Build shared library |
| `PSR_BUILD_TESTS` | ON | Build test suite |
| `PSR_BUILD_C_API` | OFF | Build C API wrapper |

## Usage

### Using with CMake

```cmake
find_package(psr_database REQUIRED)
target_link_libraries(myapp PRIVATE psr::database)
```

### C++ API

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

psr_database_t* db = psr_database_open(":memory:", NULL);
psr_database_execute(db, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)", NULL);

psr_result_t* result = psr_database_execute(db, "SELECT * FROM users", NULL);
// ... iterate over results
psr_result_free(result);
psr_database_close(db);
```

## Project Structure

```
psr_database/
├── cmake/                  # CMake modules
├── include/psr_database/   # Public C++ headers
├── src/                    # Core library implementation
├── src_c/                  # C API wrapper
└── tests/                  # GoogleTest suite
```

## License

See LICENSE file for details.
