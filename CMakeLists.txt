cmake_minimum_required(VERSION 3.15)

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(BUILD_MODE Debug)
SET(CMAKE_CXX_FLAGS "-Wall")
SET(CMAKE_CXX_FLAGS_DEBUG "-g")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3")
SET(EXECUTABLE "allocator")

project(profiling)

find_package(benchmark REQUIRED)


# Add the benchmark executable
# add_executable(${PROJECT_NAME} benchmark.cpp)


add_executable(${PROJECT_NAME} big_data_benchmark.cpp)
add_executable(${EXECUTABLE} main.cpp)

target_link_libraries(${PROJECT_NAME} benchmark::benchmark_main)
