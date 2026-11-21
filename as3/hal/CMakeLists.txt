# CMakeLists.txt for HAL
#   Builds a library (`hal`) which exposes the header files as "hal/*.h"
#   Use header as: #include "hal/joystick.h"
#
# Add each .c file here once it has real content
# (an empty .c file fails to compile under -Wpedantic -Werror).
add_library(hal STATIC
    src/joystick.c
    src/rotary_encoder.c
    # src/accelerometer.c
)
target_include_directories(hal PUBLIC include)
target_link_libraries(hal PUBLIC /usr/lib/aarch64-linux-gnu/libgpiod.a)
