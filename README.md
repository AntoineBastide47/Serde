# Serde

#### A fast, single-library C++20 parser and serialization toolkit.
> [!NOTE]
> It currently only provides JSON parsing, library and reflection to mirror python like JSON libraries and more formats
> can easily be added

---

## Building

Requirements: CMake ≥ 3.20, a C++20 compiler (Clang or GCC).

```bash
# Clone / enter the directory
cd tools/Serde

# Build (static library, Release)
./serde.sh build

# Build as shared library
./serde.sh build --shared

# Debug build (ASan + UBSan enabled)
./serde.sh build --debug

# Remove build artifacts
./serde.sh clean
```

### CMake integration

**1. Add as a submodule**

```bash
git submodule add https://github.com/AntoineBastide47/Serde.git extern/Serde
git submodule update --init --recursive
```

**2. Wire into CMake**

```cmake
add_subdirectory(extern/Serde)
target_link_libraries(my_target PRIVATE Serde)
```

---

## Quick start

### Parsing JSON

```cpp
#include "JsonParser.hpp"
#include "JSON.hpp"

// From a string_view (zero-copy fast path)
Serde::JSON json = Serde::JSONParser(R"({"name":"Alice","age":30})").Parse();

// From a file
std::ifstream f("data.json");
Serde::JSON json = Serde::JSONParser(f).Parse();

// Stream operator
Serde::JSON json;
std::cin >> json;
```

### Reading values

```cpp
std::string name = json["name"].GetString();  // "Alice"
double      age  = json["age"].GetNumber();   // 30.0

// Safe access with default
Serde::JSON missing;
const Serde::JSON &val = json.Value("missing", missing); // returns missing

// Type checks
json["name"].IsString();  // true
json["age"].IsNumber();   // true
json.IsObject();          // true
```

### Arrays

```cpp
Serde::JSON arr = Serde::JSONParser("[1, 2, 3]").Parse();

for (size_t i = 0; i < arr.Size(); ++i)
    std::cout << arr[i].GetNumber() << "\n";

// Or iterate the underlying vector directly
for (const Serde::JSON &elem : arr.GetArray())
    std::cout << elem.GetNumber() << "\n";
```

### Building JSON

```cpp
// Object literal
Serde::JSON obj = Serde::JSON::Object({
    {"name", "Bob"},
    {"scores", Serde::JSON::Array({10, 20, 30})},
    {"active", true},
    {"extra", Serde::null},
});

// Mutate
obj["name"] = "Carol";
obj.Emplace("city", "New York");
obj.Erase("extra");

// Array mutation
Serde::JSON arr = Serde::JSON::Array();
arr.PushBack(1);
arr.PushBack("hello");
arr.Insert(0, 99);  // insert at front
arr.Erase(1);       // erase by index
```

### Serializing to string / file

```cpp
std::string compact = obj.Dump();               // compact
std::string pretty  = obj.Dump(true);           // pretty-printed
std::string tabbed  = obj.Dump(true, '   ');    // 3 space indented

std::cout << obj;   // stream operator, compact
```

---

## Serialization system

Serde can automatically serialize and deserialize arbitrary C++ types through `Serializer` / `Deserializer`.

### Built-in type support

The following are supported out of the box:

| C++ type                                         | JSON representation        |
|--------------------------------------------------|----------------------------|
| `bool`                                           | `true` / `false`           |
| Arithmetic types (`int`, `float`, …)             | number                     |
| `std::string`, `std::string_view`, `const char*` | string                     |
| `std::vector`, `std::list`, `std::set`, …        | array                      |
| `std::map`, `std::unordered_map`                 | object                     |
| `std::pair<K, V>`                                | two-element array          |
| `std::tuple<Ts…>`                                | array                      |
| `std::unique_ptr<T>`, `std::shared_ptr<T>`       | value or `{}` if null      |
| `enum` / `enum class`                            | underlying integer         |
| `std::variant<Ts…>`                              | `{"index": N, "value": …}` |
| C-style arrays `T[N]`                            | array                      |

### Serializing

```cpp
#include "Serializer.hpp"

std::vector<int> nums = {1, 2, 3};

Serde::JSON        json   = Serde::Serializer::ToJson(nums);
std::string        str    = Serde::Serializer::ToJsonString(nums, /*prettyPrint=*/true);
Serde::Serializer::ToJsonToFile(nums, "out.json");
```

### Deserializing

```cpp
#include "Deserializer.hpp"

std::vector<int> nums = Serde::Deserializer::FromJsonString<std::vector<int>>("[1,2,3]");
std::vector<int> nums = Serde::Deserializer::FromJson<std::vector<int>>(json);
std::vector<int> nums = Serde::Deserializer::FromJsonFromFile<std::vector<int>>("out.json");
```

### Custom types

Inherit from `Serde::Reflectable` to make a type serializable. There are two possible paths.

#### With HeaderForge (recommended, requires clang++ for compilation)

Inherit from `Serde::Reflectable`. HeaderForge parses the header with Clang's AST and applies these rules automatically:

| Field                                        | Default behaviour                               | Override                      |
|----------------------------------------------|-------------------------------------------------|-------------------------------|
| `public`                                     | serialized                                      | `NON_SERIALIZABLE` to exclude |
| `private` / `protected`                      | skipped                                         | `SERIALIZE` to include        |
| `T*`, `T&`, `std::function`, `std::weak_ptr` | always skipped                                  | —                             |
| `std::shared_ptr<T>`, `std::unique_ptr<T>`   | skipped by HeaderForge; handle in `OnSerialize` | —                             |

Write the class **before** running HeaderForge — no macro or `#include` needed:

```cpp
// Vec2.hpp — as written by the user
#include "Reflectable.hpp"

struct Vec2 : Serde::Reflectable {
    float x;                          // serialized (public)
    float y;                          // serialized (public)
    NON_SERIALIZABLE float cached;    // excluded

private:
    SERIALIZE float flags;            // opted in explicitly
};
```

HeaderForge then patches `Vec2.hpp` in place — inserting `#include "Vec2.gen.hpp"` and `SERIALIZE_VEC2` after the opening brace — and writes `Vec2.gen.hpp` alongside it. Re-run HeaderForge (or wire it as a pre-build step) whenever the class definition changes.

#### Without HeaderForge

Implement `_e_save` / `_e_load` manually and provide the class-name overrides yourself:

```cpp
#include "Reflectable.hpp"
#include "Save.hpp"
#include "Load.hpp"

struct Vec2 : Serde::Reflectable {
    float x, y;

    std::string_view ClassNameQualified() const override { return "Vec2"; }
    std::string_view ClassName()          const override { return "Vec2"; }

    void _e_save(Serde::Format fmt, Serde::JSON &json) const override {
        json = Serde::JSON::Object();
        _e_saveImpl(x, fmt, json["x"]);
        _e_saveImpl(y, fmt, json["y"]);
    }

    void _e_load(Serde::Format fmt, const Serde::JSON &json) override {
        _e_loadImpl(x, fmt, json["x"]);
        _e_loadImpl(y, fmt, json["y"]);
    }
};
```

#### Usage (both paths)

```cpp
Vec2 v{1.5f, 2.5f};
std::string s  = Serde::Serializer::ToJsonString(v, true);
Vec2 v2        = Serde::Deserializer::FromJsonString<Vec2>(s);
```

#### `OnSerialize` / `OnDeserialize` hooks

Both paths call optional virtual hooks after the main save/load. Override them to handle anything the generated code cannot — extra fields, smart-pointer members HeaderForge skips, or post-load recomputation:

```cpp
struct Node : Serde::Reflectable {
    std::string name;                      // serialized automatically
    std::shared_ptr<Node> child;           // skipped by HeaderForge

    // Called after _e_save; json already contains { "name": "..." }
    void OnSerialize(Serde::Format fmt, Serde::JSON &json) const override {
        if (child)
            _e_saveImpl(*child, fmt, json["child"]);
    }

    // Called after _e_load; name is already restored
    void OnDeserialize(Serde::Format fmt, const Serde::JSON &json) override {
        if (json.Contains("child")) {
            child = std::make_shared<Node>();
            _e_loadImpl(*child, fmt, json.At("child"));
        }
    }
};
```

---

## Testing

Runs the full [JSONTestSuite](https://github.com/nst/JSONTestSuite) acceptance test suite (fetched automatically via CMake):

```bash
./serde.sh test
```

---

## Benchmarking

Runs 5 warmup + 100 measured iterations and reports min / median / p99 / max latency and throughput:

```bash
./serde.sh bench
```

Example output (Apple M3 Max, Release):

```
file                                    min     median        p99        max   throughput
-----------------------------------------------------------------------------------------
config-lockfile.json                 7.67 ms     8.04 ms     9.10 ms     9.10 ms     622.1 MB/s
duplicate-keys.json                  6.50 ms     6.83 ms     8.41 ms     8.41 ms     732.4 MB/s
escaped-strings.json                 9.39 ms     9.63 ms    10.65 ms    10.65 ms     519.0 MB/s
large-array-objects.json             7.14 ms     7.40 ms     9.18 ms     9.18 ms     675.6 MB/s
large-array-people-records.json      4.44 ms     4.55 ms     5.42 ms     5.42 ms    1076.5 MB/s
large-object-many-keys.json          5.84 ms     6.07 ms     7.80 ms     7.80 ms     823.4 MB/s
nested.json                          0.59 ms     0.61 ms     0.80 ms     0.80 ms    8232.8 MB/s
numbers-heavy.json                  13.67 ms    14.10 ms    16.16 ms    16.16 ms     354.6 MB/s
trace-events.json                   13.52 ms    13.90 ms    16.57 ms    16.57 ms     359.8 MB/s
unicode-strings.json                 2.90 ms     3.05 ms     3.62 ms     3.62 ms    1639.0 MB/s
```

Example output (GitHub CI 2 core x86_64 ~2-3GHz, Release):
```
file                                    min     median        p99        max   throughput
-----------------------------------------------------------------------------------------
config-lockfile.json                16.23 ms    22.17 ms    23.01 ms    23.01 ms     225.5 MB/s
duplicate-keys.json                 21.95 ms    22.19 ms    23.49 ms    23.49 ms     225.3 MB/s
escaped-strings.json                12.76 ms    12.80 ms    15.12 ms    15.12 ms     390.5 MB/s
large-array-objects.json            24.03 ms    24.25 ms    26.65 ms    26.65 ms     206.2 MB/s
large-array-people-records.json     10.32 ms    10.41 ms    10.66 ms    10.66 ms     469.9 MB/s
large-object-many-keys.json         19.08 ms    19.26 ms    20.14 ms    20.14 ms     259.7 MB/s
nested.json                          1.26 ms     1.27 ms     1.31 ms     1.31 ms    3972.8 MB/s
numbers-heavy.json                  38.50 ms    38.70 ms    39.54 ms    39.54 ms     129.2 MB/s
trace-events.json                   28.70 ms    29.19 ms    44.57 ms    44.57 ms     171.3 MB/s
unicode-strings.json                 5.89 ms     5.93 ms     6.05 ms     6.05 ms     843.2 MB/s
```

---

## serde.sh reference

```
Usage: serde.sh [command] [options]

Commands:
  build       Build the Serde library (default)
  test        Build and run the JSONTestSuite
  bench       Build and run the benchmark
  all         Build, test, then benchmark
  clean       Remove the build directory

Options:
  --debug         Build in Debug mode with ASan + UBSan (default: Release)
  --shared        Build as shared library (default: static)
  --jobs N        Parallel build jobs (default: auto-detected)
  --build-dir DIR Override build directory (default: ./build)
  -h, --help      Show this help
```
