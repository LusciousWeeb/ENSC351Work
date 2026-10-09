# Build the reaction timer application
include_directories(include)


add_executable(reaction_timer src/main.c)

# Link the hardware abstraction layer
target_link_libraries(reaction_timer PRIVATE hal)