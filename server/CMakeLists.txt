cmake_minimum_required(VERSION 3.15)
project(cube_streamer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-webrtc-1.0 gstreamer-sdp-1.0)
pkg_check_modules(WS REQUIRED libwebsockets)

find_library(EGL_LIB EGL)
find_library(GLESV2_LIB GLESv2)

add_executable(cube_server
    src/main.c
    src/render_egl.c
    src/pipeline_gst.c
    src/signaling_ws.c
    src/config.c
)

target_include_directories(cube_server PRIVATE
    include
    ${GST_INCLUDE_DIRS}
    ${WS_INCLUDE_DIRS}
)

target_compile_options(cube_server PRIVATE ${GST_CFLAGS_OTHER} ${WS_CFLAGS_OTHER})

target_link_libraries(cube_server
    ${GST_LIBRARIES}
    ${WS_LIBRARIES}
    ${EGL_LIB}
    ${GLESV2_LIB}
    m
)
