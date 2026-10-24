cmake_minimum_required(VERSION 3.25)
# Optimise by default: the xsdk toolchain file sets no -O level, and an unoptimised build is too slow.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
project(zelda3_xbox360 C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_FLAGS_RELEASE "-O2")
# The game reads bytes as words throughout (as on the PC build); keep the optimiser from assuming otherwise.
add_compile_options(-fno-strict-aliasing)
set(ZELDA_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(ZELDA_SDL_XENON_DIR "$ENV{HOME}/SDL-xenon" CACHE PATH "Path to the external SDL-xenon checkout")
# SDL-xenon's build folder holds its CMake package, as its test/xsdk title uses (-DSDL3_DIR=build-xsdk).
set(SDL3_DIR "${ZELDA_SDL_XENON_DIR}/build-xsdk" CACHE PATH "SDL-xenon build folder")
find_package(SDL3 REQUIRED CONFIG)

# ---- hqx (LGPL 2.1, platform/hqx/COPYING): the HQ2x and HQ4x filters, kept a library of their own ----
add_library(snes360_hqx STATIC platform/hqx/hq2x.c platform/hqx/hq4x.c platform/hqx/init.c)
target_compile_options(snes360_hqx PRIVATE -O3)

# ---- snes360: the reusable Xbox 360 platform layer for SNES ports (video, audio, input, settings, menu) ----
add_library(snes360 STATIC
  platform/s360_main.c platform/s360_video.c platform/s360_audio.c platform/s360_input.c
  platform/s360_settings.c platform/s360_draw.c platform/s360_menu.c platform/s360_jobs.c)
target_include_directories(snes360 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/platform")
target_compile_options(snes360 PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(snes360 PRIVATE S360_HAVE_XSDK=1)
target_link_libraries(snes360 PUBLIC snes360_hqx SDL3::SDL3-static)
xsdk_add_shaders(snes360 platform/blit.vert platform/blit.frag)

# ---- zelda3's game core, unchanged from the PC build apart from big-endian support ----
file(GLOB ZELDA_CORE CONFIGURE_DEPENDS "${ZELDA_ROOT}/src/*.c" "${ZELDA_ROOT}/snes/*.c")
list(REMOVE_ITEM ZELDA_CORE "${ZELDA_ROOT}/src/main.c" "${ZELDA_ROOT}/src/opengl.c" "${ZELDA_ROOT}/src/glsl_shader.c")
list(APPEND ZELDA_CORE "${ZELDA_ROOT}/third_party/gl_core/gl_core_3_1.c" "${ZELDA_ROOT}/third_party/opus-1.3.1-stripped/opus_decoder_amalgam.c")
add_library(zelda_xenon_core STATIC ${ZELDA_CORE})
target_include_directories(zelda_xenon_core PRIVATE "${ZELDA_ROOT}" "${ZELDA_ROOT}/src" "${ZELDA_ROOT}/third_party/opus-1.3.1-stripped" "${ZELDA_SDL_XENON_DIR}/include" "${ZELDA_SDL_XENON_DIR}/include/SDL3")
target_compile_options(zelda_xenon_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_compile_definitions(zelda_xenon_core PRIVATE ZELDA_XENON=1 SDL_ENABLE_OLD_NAMES=1)

# ---- The title: zelda3's description for snes360 ----
add_executable(zelda3_xbox360 zelda3/zelda3_port.c)
target_include_directories(zelda3_xbox360 PRIVATE "${ZELDA_ROOT}" "${ZELDA_ROOT}/src")
target_compile_definitions(zelda3_xbox360 PRIVATE ZELDA_XENON=1)
target_compile_options(zelda3_xbox360 PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(zelda3_xbox360 PRIVATE snes360 zelda_xenon_core snes360 SDL3::SDL3-static m)
xsdk_add_xex(zelda3_xbox360 NAME "Zelda: A Link to the Past")
