file(GLOB MY_SOURCES "src/*.c")
add_library(hal STATIC ${MY_SOURCES})
target_include_directories(hal PUBLIC include)
target_link_libraries(hal PUBLIC /usr/lib/aarch64-linux-gnu/libgpiod.a)
