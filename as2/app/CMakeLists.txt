add_executable(light_sampler
    src/main.c
    src/periodTimer.c
    src/sampler.c
    src/dip_detector.c
    src/terminal_out.c
    src/udp_server.c
    src/led_control.c
)
target_include_directories(light_sampler PRIVATE include)
target_link_libraries(light_sampler PRIVATE hal)