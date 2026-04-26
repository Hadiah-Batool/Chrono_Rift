cmake_minimum_required(VERSION 3.10)
project(sfml_test_logic)

# Set C++17 for thread support
set(CMAKE_CXX_STANDARD 17)

# This finds the Linux libraries you installed via 'apt'
find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

# Tell CMake where your code is
add_executable(sfml_app src/main.cpp)

# Link the libraries (the Linux way)
target_link_libraries(sfml_app sfml-graphics sfml-window sfml-system)
