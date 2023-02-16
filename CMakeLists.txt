# Configure CMake
cmake_minimum_required(VERSION 3.5.1)
project(radar-fun CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wextra -Wshadow -Wpointer-arith -Wcast-qual -Wformat=2 -Weffc++ -Werror")

# Add libraries
find_package(UHD 4.1.0 REQUIRED)

# Add "simple" executable
add_executable(simple simple.cc)
target_link_libraries(simple ${UHD_LIBRARIES})
