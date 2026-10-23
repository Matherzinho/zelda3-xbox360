cmake_minimum_required(VERSION 3.25)
project(zelda3_xbox360 C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(ZELDA_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(ZELDA_SDL_XENON_DIR "$ENV{HOME}/SDL-xenon" CACHE PATH "Path to the external SDL-xenon checkout")

# Build the Xenon Xenos presentation backend and frontend without changing the PC Makefile.
add_library(zelda_xenon_renderer OBJECT renderer.c)
target_include_directories(zelda_xenon_renderer PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${ZELDA_ROOT}/src" "${ZELDA_ROOT}")
target_compile_options(zelda_xenon_renderer PRIVATE -Wall -Wextra -Werror)
xsdk_add_shaders(zelda_xenon_renderer blit.vert blit.frag)

file(GLOB ZELDA_CORE CONFIGURE_DEPENDS "${ZELDA_ROOT}/src/*.c" "${ZELDA_ROOT}/snes/*.c")
list(REMOVE_ITEM ZELDA_CORE "${ZELDA_ROOT}/src/main.c" "${ZELDA_ROOT}/src/opengl.c" "${ZELDA_ROOT}/src/glsl_shader.c")
list(APPEND ZELDA_CORE "${ZELDA_ROOT}/third_party/gl_core/gl_core_3_1.c" "${ZELDA_ROOT}/third_party/opus-1.3.1-stripped/opus_decoder_amalgam.c")
add_library(zelda_xenon_core STATIC ${ZELDA_CORE})
target_include_directories(zelda_xenon_core PRIVATE "${ZELDA_ROOT}" "${ZELDA_ROOT}/src" "${ZELDA_ROOT}/third_party/opus-1.3.1-stripped" "${ZELDA_SDL_XENON_DIR}/include" "${ZELDA_SDL_XENON_DIR}/include/SDL3")
target_compile_options(zelda_xenon_core PRIVATE -Wall -Wextra -Wno-unused-parameter)


target_compile_definitions(zelda_xenon_core PRIVATE ZELDA_XENON=1)
target_compile_definitions(zelda_xenon_core PRIVATE SDL_ENABLE_OLD_NAMES=1)

add_executable(zelda3_xbox360 main.c $<TARGET_OBJECTS:zelda_xenon_renderer>)
target_include_directories(zelda3_xbox360 PRIVATE "${ZELDA_ROOT}" "${ZELDA_ROOT}/src" "${CMAKE_CURRENT_BINARY_DIR}" "${ZELDA_SDL_XENON_DIR}/include" "${ZELDA_SDL_XENON_DIR}/include/SDL3")
target_compile_options(zelda3_xbox360 PRIVATE -Wall -Wextra)
target_link_libraries(zelda3_xbox360 PRIVATE zelda_xenon_core ${ZELDA_SDL_XENON_DIR}/build-xsdk/libSDL3.a m)
xsdk_add_xex(zelda3_xbox360)
