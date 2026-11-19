cmake_minimum_required(VERSION 3.22.1)
project(lr C)
# The game, exactly as it runs on the watch, linked as the monolith (every
# domain compiled in - a phone has no need of the overlays). Nothing here is
# phone-only except the platform layer.
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)
file(GLOB KERNEL ${ROOT}/src/c/wr_*.c)
file(GLOB DOMAINS ${ROOT}/src/domains/dom_*.c)
# (src/domains/wr_domain.c is a mirror of src/c/wr_domain.c for the blob
# builder; the glob above already has the kernel's copy.)
add_library(lr SHARED
    ${KERNEL} ${DOMAINS}
    pebble_android.c glue.c
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(lr PRIVATE
    ${ROOT}/src/c ${ROOT}/src/domains ${ROOT}/tools/hostcheck
    ${ANDROID_NDK}/sources/android/native_app_glue)
# The phone renders at three times the watch's resolution: the projection and
# the touch hit-boxes scale with SCR_H (see PX() in wr.h).
target_compile_definitions(lr PRIVATE WR_MONOLITH=1 WR_LEAN WR_DOMAINS=1 WR_BOATS=0 WR_TAXI=1 WR_GOD=0
    WR_ANDROID=1 SCR_H=684)
target_compile_options(lr PRIVATE -O2 -std=c11 -Wall -Wno-unused-function -Wno-unused-variable)
target_link_libraries(lr android log OpenSLES m)
# 16 KB page alignment: Android 15+ wants it, and warns in debuggable builds without it.
set_target_properties(lr PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate -Wl,-z,max-page-size=16384")
