# CMake Build Configuration for root of project
cmake_minimum_required(VERSION 3.18)
project(light_sampler VERSION 1.0 DESCRIPTION "ENSC 351 Assignment 2" LANGUAGES C)

# Compiler options (inherited by sub-folders)
set(CMAKE_C_STANDARD 99)
add_compile_options(-Wall -Werror -Wpedantic -Wextra -g -D_POSIX_C_SOURCE=200809L)
add_compile_options(-fdiagnostics-color)

# Enable address sanitizer
# (Comment this out to make your code faster)
add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

# Enable PThread library for linking
add_compile_options(-pthread)
add_link_options(-pthread)

# What folders to build
add_subdirectory(hal)  
add_subdirectory(app)

# Link libraries (Must be placed AFTER add_subdirectory(app) creates the target)
find_package(Threads REQUIRED)
target_link_libraries(light_sampler PRIVATE hal Threads::Threads gpiod)