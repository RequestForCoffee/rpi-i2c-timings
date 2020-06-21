cmake_minimum_required(VERSION 3.10)
project(rpi-i2c-timings)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(abseil-cpp)

add_executable(rpi-i2c rpi_i2c.cpp)

target_include_directories(rpi-i2c PUBLIC /opt/vc/include)
target_link_directories(rpi-i2c PUBLIC /opt/vc/lib)
target_link_libraries(rpi-i2c atomic bcm_host absl::base absl::flags absl::strings absl::time)
