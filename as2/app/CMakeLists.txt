add_executable(light_sampler src/main.c)
target_include_directories(light_sampler PRIVATE include)
target_link_libraries(light_sampler PRIVATE hal)
