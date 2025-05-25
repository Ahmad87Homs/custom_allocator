cmake_minimum_required(VERSION 3.15)

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(BUILD_MODE Debug)
SET(CMAKE_CXX_FLAGS "-Wall")
SET(CMAKE_CXX_FLAGS_DEBUG "-g")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3")

SET(STATIC_BENCHMARK "static_allocator")
SET(STATIC_EXECUTABLE "static_allocator_profiling")
SET(DYNAMIC_BENCHMARK "dynamic_allocator_profiling")
SET(DYNAMIC_EXECUTABLE "dynamic_allocator")

project(profiling)

find_package(benchmark REQUIRED)


# Add the benchmark executable
# add_executable(${PROJECT_NAME} benchmark.cpp)


add_executable(${STATIC_BENCHMARK} Static/big_data_benchmark.cpp)
add_executable(${STATIC_EXECUTABLE} Static/main.cpp)
add_executable(${DYNAMIC_BENCHMARK} Dynamic/big_data_benchmark.cpp)
add_executable(${DYNAMIC_EXECUTABLE} Dynamic/main.cpp)

target_link_libraries(${STATIC_BENCHMARK} benchmark::benchmark_main)
target_link_libraries(${DYNAMIC_BENCHMARK} benchmark::benchmark_main)
