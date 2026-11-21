cmake_minimum_required(VERSION 3.18)
project(beatbox VERSION 1.0 DESCRIPTION "ENSC 351 Assignment 3: Beat-Box" LANGUAGES C)
set(CMAKE_C_STANDARD 99)

# Compiler options (inherited by sub-folders)
add_compile_options(-Wall -Werror -Wpedantic -Wextra -g -D_POSIX_C_SOURCE=200809L)
add_compile_options(-fdiagnostics-color)

# Address sanitizer (leave off when running under Valgrind)
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

# PThread
add_compile_options(-pthread)
add_link_options(-pthread)

# What folders to build
add_subdirectory(hal)
add_subdirectory(app)
