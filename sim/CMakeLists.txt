# PixelLoop desktop simulator: the OS app's UI sources compiled against LVGL
# (same version as the firmware, fetched at configure time) with the SDL2
# window driver. Speaks the device console protocol on stdin/stdout.
#
#   cmake -S sim -B sim/build -G Ninja [-DPIXELLOOP_BOARD=ws169]
#   cmake --build sim/build
#   sim/build/pixelloop-sim [--headless] [--zoom 2]
#
# Prefer `make sim`, `make sim-shot`, `make sim-test` from the repo root.
cmake_minimum_required(VERSION 3.24)
project(pixelloop-sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(PIXELLOOP_BOARD "ws169" CACHE STRING "board directory under boards/ whose panel geometry is simulated")
set(REPO ${CMAKE_CURRENT_LIST_DIR}/..)
set(FW ${REPO}/firmware)
set(OS ${FW}/src/apps/os)
set(BOARD_DIR ${REPO}/boards/${PIXELLOOP_BOARD})
if(NOT EXISTS ${BOARD_DIR}/board.h)
  message(FATAL_ERROR "PIXELLOOP_BOARD='${PIXELLOOP_BOARD}': no ${BOARD_DIR}/board.h")
endif()

# One version string for device and simulator: read it from platformio.ini.
file(STRINGS ${FW}/platformio.ini _ver_line REGEX "PIXELLOOP_VERSION=")
string(REGEX MATCH "PIXELLOOP_VERSION=\\\\\"([0-9A-Za-z.+-]+)" _m "${_ver_line}")
if(NOT CMAKE_MATCH_1)
  message(FATAL_ERROR "PIXELLOOP_VERSION not found in ${FW}/platformio.ini")
endif()
set(PIXELLOOP_VERSION ${CMAKE_MATCH_1})

# ---- LVGL: same major.minor.patch as firmware/dependencies.lock ----------
file(STRINGS ${FW}/dependencies.lock _lock_lines)
set(_in_lvgl OFF)
foreach(_l IN LISTS _lock_lines)
  if(_l MATCHES "^  lvgl/lvgl:")
    set(_in_lvgl ON)
  elseif(_l MATCHES "^  [a-z]")
    set(_in_lvgl OFF)
  elseif(_in_lvgl AND _l MATCHES "^    version: ([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(LVGL_VERSION ${CMAKE_MATCH_1})
  endif()
endforeach()
if(NOT LVGL_VERSION)
  message(FATAL_ERROR "lvgl/lvgl version not found in ${FW}/dependencies.lock")
endif()
message(STATUS "LVGL v${LVGL_VERSION} (from firmware/dependencies.lock), board ${PIXELLOOP_BOARD}, version ${PIXELLOOP_VERSION}")

include(FetchContent)
FetchContent_Declare(lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG        v${LVGL_VERSION}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE)
# LVGL reads its configuration the way the ESP-IDF build does: from Kconfig
# macros (sim/lv_kconfig.h, generated from firmware/sdkconfig.<board>) plus the
# simulator-only SDL options below. No lv_conf.h anywhere.
set(LV_CONF_SKIP ON CACHE BOOL "" FORCE)
set(LV_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lvgl)

find_package(SDL2 REQUIRED CONFIG)
if(TARGET SDL2::SDL2)
  set(SDL2_TARGET SDL2::SDL2)
else()
  set(SDL2_TARGET ${SDL2_LIBRARIES})
  target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
endif()

target_include_directories(lvgl PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(lvgl PUBLIC
  LV_CONF_KCONFIG_EXTERNAL_INCLUDE="lv_kconfig.h"
  CONFIG_LV_USE_SDL=1
  CONFIG_LV_SDL_DIRECT_EXIT=1)
target_link_libraries(lvgl PUBLIC ${SDL2_TARGET})

# Drift check: when the firmware's generated sdkconfig exists locally, the
# committed header must match it (`make sim-lvconf` regenerates it).
find_package(Python3 COMPONENTS Interpreter)
set(_sdkconfig ${FW}/sdkconfig.${PIXELLOOP_BOARD})
if(Python3_Interpreter_FOUND AND EXISTS ${_sdkconfig})
  execute_process(
    COMMAND ${Python3_EXECUTABLE} ${REPO}/tools/lvconf.py --sdkconfig ${_sdkconfig} --check ${CMAKE_CURRENT_LIST_DIR}/lv_kconfig.h
    RESULT_VARIABLE _rc)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "sim/lv_kconfig.h does not match ${_sdkconfig}: run `make sim-lvconf`")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_sdkconfig})
endif()

# ---- The OS, unchanged, plus the host port layer -------------------------
# These sources may include only lvgl.h, libc and the port layer. Anything
# ESP-specific fails to compile here, which is the point.
set(OS_SOURCES
  ${OS}/ui.c
  ${OS}/cmds.c
  ${OS}/screenshot.c
  ${OS}/synth_input.c
  ${FW}/src/common/console.c)

add_executable(pixelloop-sim
  src/main.c
  src/pl_port_host.c
  ${OS_SOURCES})
target_include_directories(pixelloop-sim PRIVATE
  ${OS}
  ${FW}/src/common
  ${BOARD_DIR})
target_compile_definitions(pixelloop-sim PRIVATE
  PIXELLOOP_VERSION="${PIXELLOOP_VERSION}"
  PIXELLOOP_APP_NAME="os"
  PIXELLOOP_BOARD_NAME="${PIXELLOOP_BOARD}"
  _GNU_SOURCE)
target_compile_options(pixelloop-sim PRIVATE -Wall -Wextra -Wno-unused-parameter)
find_package(Threads REQUIRED)
target_link_libraries(pixelloop-sim PRIVATE lvgl ${SDL2_TARGET} Threads::Threads m)
