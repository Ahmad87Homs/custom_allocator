# BigData Benchmark

A benchmark project for evaluating a custom allocator used with `std::map` to store large periodic data.

## 📦 Dependencies

- [CMake](https://cmake.org/) >= 3.10
- [Conan](https://conan.io/) >= 1.50
- A C++11 compatible compiler (e.g., GCC, Clang, MSVC)

## 🔧 Setup Instructions

### 1. Build the Project
```bash
conan profile detect --force
mkdir build
cd build
conan install .. --output-folder=build --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=build/build/Release/generators/conan_toolchain.cmake  -DCMAKE_BUILD_TYPE=Release
cmake --build .
./profiling