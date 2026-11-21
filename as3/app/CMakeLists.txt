# CMakeLists.txt for the beatbox application
#
# Add each .c file here once it has real content
# (an empty .c file fails to compile under -Wpedantic -Werror).
add_executable(beatbox
    src/main.c
    src/audioMixer.c
    src/periodTimer.c
    # src/beatPlayer.c
    # src/inputs.c
    # src/udpServer.c
    # src/display.c
    # src/shutdown.c
)
target_include_directories(beatbox PRIVATE include)
target_link_libraries(beatbox PRIVATE hal)

# ALSA (uses the arm64 libasound installed on the host per the Audio Guide)
find_package(ALSA REQUIRED)
target_link_libraries(beatbox PRIVATE asound)

# Deploy to the NFS folder (assignment section 2).
# Uses $ENV{HOME} so it works for any user / unzip location.
set(MYAPPS "$ENV{HOME}/ensc351/public/myApps")
add_custom_command(TARGET beatbox POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MYAPPS}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:beatbox>" "${MYAPPS}/beatbox"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_SOURCE_DIR}/wave-files" "${MYAPPS}/beatbox-wav-files"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_SOURCE_DIR}/as3-server" "${MYAPPS}/beatbox-server-copy"
    COMMENT "Deploying beatbox, WAV files and Node server to ${MYAPPS}")
