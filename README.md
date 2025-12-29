# psr_database

A cross-platform C++ SQLite wrapper library with bindings for Python, Julia, and Dart.

## Features

- Modern C++17 API with RAII resource management
- Cross-platform support (Windows, Linux, macOS)
- SQLite embedded via CMake FetchContent
- Language bindings:
  - **Python** - pybind11 bindings
  - **Julia** - Clang.jl-based ccall bindings
  - **Dart** - dart:ffi bindings
- C API for FFI integration with other languages

## Building

### Prerequisites

- CMake 3.21 or higher
- C++17 compatible compiler:
  - GCC 8+ / Clang 7+ (Linux/macOS)
  - MSVC 2019+ (Windows)

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
| `PSR_BUILD_EXAMPLES` | OFF | Build examples |
| `PSR_BUILD_C_API` | OFF | Build C API wrapper |
| `PSR_BUILD_PYTHON_BINDING` | OFF | Build Python binding |

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

### Python

```python
from psr_database import Database

db = Database(":memory:")
db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
db.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])

result = db.execute("SELECT * FROM users")
for row in result:
    print(row[1])
```

### Julia

```julia
using PsrDatabase

db = Database(":memory:")
execute(db, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
execute(db, "INSERT INTO users (name) VALUES ('Alice')")

result = execute(db, "SELECT * FROM users")
for row in result
    println(row["name"])
end
```

### Dart

```dart
import 'package:psr_database/psr_database.dart';

final db = Database.open(':memory:');
db.execute('CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)');
db.execute("INSERT INTO users (name) VALUES ('Alice')");

final result = db.execute('SELECT * FROM users');
for (final row in result.toList()) {
    print(row['name']);
}
```

## Testing

### C++ Tests

```bash
# Build with tests enabled
cmake -B build -DPSR_BUILD_TESTS=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

### Python Tests

```bash
# Build Python binding first
cmake -B build -DPSR_BUILD_PYTHON_BINDING=ON
cmake --build build

# Run tests with uv
cd bindings/python
uv sync
uv run pytest tests/ -v
```

### Julia Tests

```bash
# Build C API first
cmake -B build -DPSR_BUILD_C_API=ON
cmake --build build

# Set library path (Linux/macOS)
export LD_LIBRARY_PATH=$PWD/build/lib:$LD_LIBRARY_PATH

# Run tests
julia --project=bindings/julia/PsrDatabase -e "using Pkg; Pkg.instantiate(); Pkg.test()"
```

### Dart Tests

```bash
# Build C API first
cmake -B build -DPSR_BUILD_C_API=ON
cmake --build build

# Set library path (Linux/macOS)
export LD_LIBRARY_PATH=$PWD/build/lib:$LD_LIBRARY_PATH

# Run tests
cd bindings/dart
dart pub get
dart test
```

## Project Structure

```
psr_database/
├── cmake/                  # CMake modules
├── include/psr_database/   # Public C++ headers
├── src/                    # Core library implementation
├── src_c/                  # C API wrapper
├── bindings/
│   ├── python/             # Python binding (pybind11)
│   │   └── tests/          # pytest tests
│   ├── julia/              # Julia binding (ccall)
│   │   └── test/           # Julia tests
│   └── dart/               # Dart binding (FFI)
│       └── test/           # Dart tests
├── tests/                  # C++ test suite (GoogleTest)
└── examples/               # Usage examples
```

## License

See LICENSE file for details.
