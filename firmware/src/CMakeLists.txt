# Main component. Which app and which board go into the image is decided by
# two CMake cache variables set from platformio.ini (board_build.cmake_extra_args):
#   PIXELLOOP_APP    directory under src/apps/   (probe, os, ...)
#   PIXELLOOP_BOARD  directory under ../boards/  (generic, ws169, ...)
set(PIXELLOOP_APP   "probe"   CACHE STRING "app directory under src/apps")
set(PIXELLOOP_BOARD "generic" CACHE STRING "board directory under boards/")

file(GLOB common_sources ${CMAKE_CURRENT_LIST_DIR}/common/*.c)
file(GLOB app_sources    ${CMAKE_CURRENT_LIST_DIR}/apps/${PIXELLOOP_APP}/*.c)
file(GLOB board_sources  ${CMAKE_CURRENT_LIST_DIR}/../../boards/${PIXELLOOP_BOARD}/*.c)

if(NOT app_sources)
  message(FATAL_ERROR "PIXELLOOP_APP='${PIXELLOOP_APP}' has no sources under src/apps/")
endif()

idf_component_register(
  SRCS ${common_sources} ${app_sources} ${board_sources}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/common
    ${CMAKE_CURRENT_LIST_DIR}/../../boards/${PIXELLOOP_BOARD}
  REQUIRES
    esp_driver_i2c esp_driver_gpio esp_driver_spi esp_driver_ledc
    esp_driver_usb_serial_jtag esp_lcd esp_psram spi_flash esp_app_format
    esp_timer esp_hw_support nvs_flash vfs mbedtls
    lvgl esp_lvgl_port esp_lcd_touch_cst816s
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  PIXELLOOP_APP_NAME="${PIXELLOOP_APP}"
  PIXELLOOP_BOARD_NAME="${PIXELLOOP_BOARD}")
