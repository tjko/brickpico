# CMakeLists.txt for BrickPico
#

cmake_minimum_required(VERSION 3.13)

# Include Pico-SDK ($PICO_SDK_PATH must be set)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)


project(brickpico
  VERSION 1.3.0
  LANGUAGES C CXX ASM
  )
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(FANPICO_BUILD beta CACHE STRING "Fanpico Build Tag")
set(BRICKPICO_BOARD 8 CACHE STRING "Brickpico Board Model")
set(PICO_BOARD pico_w CACHE STRING "Pico Board")
set(TLS_SUPPORT 1 CACHE STRING "TLS Support")

# Generate some "random" data for mbedtls (better than nothing...)
set(EXTRA_ENTROPY_LEN 64)
string(RANDOM LENGTH ${EXTRA_ENTROPY_LEN} EXTRA_ENTROPY)

#set(CMAKE_BUILD_TYPE Debug)


message("---------------------------------")
message("     BRICKPICO_BOARD: ${BRICKPICO_BOARD}")
message("          PICO_BOARD: ${PICO_BOARD}")
message("       PICO_PLATFORM: ${PICO_PLATFORM}")
message("         TLS_SUPPORT: ${TLS_SUPPORT}")
message("    CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("---------------------------------")

pico_sdk_init()


# External libaries

# pico-telnetd
add_subdirectory(libs/pico-telnetd)

# pico-lfs
add_subdirectory(libs/pico-lfs)
# prevent warnings from littlefs...
set_source_files_properties(
  libs/pico-lfs/libs/littlefs/lfs.c
  PROPERTIES
  COMPILE_OPTIONS "-Wno-unused-function"
  )

# ss_oled
add_subdirectory(libs/ss_oled-lib)

# bb_spi_lcd
#add_subdirectory(libs/bb_spi_lcd-lib)

# cJSON
add_library(cJSON INTERFACE)
target_include_directories(cJSON INTERFACE libs/cJSON)
target_sources(cJSON INTERFACE
  libs/cJSON/cJSON.h
  libs/cJSON/cJSON.c
  )

# libb64
add_library(libb64 INTERFACE)
target_include_directories(libb64 INTERFACE libs/libb64/include)
target_sources(libb64 INTERFACE
  libs/libb64/src/cdecode.c
  libs/libb64/src/cencode.c
  )


# brickpico firmware

add_executable(brickpico
  src/brickpico.c
  src/bi_decl.c
  src/command.c
  src/flash.c
  src/config.c
  src/display.c
  src/display_oled.c
  src/network.c
  src/timer.c
  src/tls.c
  src/pwm.c
  src/temp.c
  src/util.c
  src/util_rp2040.c
  src/log.c
  src/crc32.c
  src/ringbuffer.c
  src/credits.s
  )

set_property(SOURCE src/*.s APPEND PROPERTY COMPILE_OPTIONS -I${CMAKE_CURRENT_LIST_DIR}/src)
set_property(SOURCE src/credits.s APPEND PROPERTY COMPILE_OPTIONS -I${CMAKE_CURRENT_LIST_DIR})


pico_enable_stdio_usb(brickpico 1)
pico_enable_stdio_uart(brickpico 0)
pico_add_extra_outputs(brickpico)

target_include_directories(brickpico PRIVATE src)

configure_file(src/config.h.in config.h)
configure_file(src/brickpico-compile.h.in brickpico-compile.h)

target_link_libraries(brickpico PRIVATE
  pico_stdlib
  pico_stdio_uart
  pico_multicore
  pico_rand
  pico_aon_timer
  hardware_pwm
  hardware_i2c
  hardware_rtc
  hardware_adc
  pico-lfs
  cJSON
  ss_oled-lib
  libb64
  )

if (PICO_CYW43_SUPPORTED) # set by PICO_BOARD=pico_w
  message("Enabling WiFi stuff...")
  target_sources(brickpico PRIVATE
    src/syslog.c
    src/httpd.c
    src/mqtt.c
    src/telnetd.c
    )
  target_link_libraries(brickpico PRIVATE
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_sntp
    pico_lwip_http
    pico_lwip_mqtt
    pico-telnetd-lib
    )
  if (TLS_SUPPORT)
    target_link_libraries(brickpico PRIVATE
      pico_lwip_mbedtls
      pico_mbedtls
      )
    # prevent warnings from mbedtls...
    set_source_files_properties(
      ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls.c
      PROPERTIES
      COMPILE_OPTIONS "-Wno-unused-result"
      )
  endif()
endif()


target_compile_options(brickpico PRIVATE -Wall)
target_compile_options(brickpico PRIVATE -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

target_compile_definitions(brickpico PRIVATE USBD_MANUFACTURER="TJKO Industries")
target_compile_definitions(brickpico PRIVATE USBD_PRODUCT="BrickPico-${BRICKPICO_BOARD} LED Controller")
target_compile_definitions(brickpico PRIVATE USBD_DESC_STR_MAX=32)
target_compile_definitions(brickpico PRIVATE PARAM_ASSERTIONS_ENABLE_ALL=1)
target_compile_definitions(brickpico PRIVATE PICO_MALLOC_PANIC=0)
target_compile_definitions(brickpico PRIVATE PICO_DEBUG_MALLOC=0)
target_compile_definitions(brickpico PRIVATE PICO_DEOPTIMIZED_DEBUG=0)
target_compile_definitions(brickpico PRIVATE PICO_STACK_SIZE=0x2000)
target_compile_definitions(brickpico PRIVATE PICO_CORE1_STACK_SIZE=0x1000)
target_compile_definitions(brickpico PRIVATE PICO_USE_STACK_GUARDS=1)
target_compile_definitions(brickpico PRIVATE PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)

target_compile_definitions(brickpico PRIVATE
  LFS_THREADSAFE=1
  LFS_NO_DEBUG=1
#  LFS_YES_TRACE=1
  )

if (${PICO_SDK_VERSION_MAJOR} LESS 2)
  pico_set_linker_script(brickpico ${CMAKE_SOURCE_DIR}/src/memmap_custom.ld)
else()
  if (${PICO_PLATFORM} MATCHES rp2350)
    pico_set_linker_script(brickpico ${CMAKE_SOURCE_DIR}/src/memmap_custom_rp2350.ld)
  else()
    pico_set_linker_script(brickpico ${CMAKE_SOURCE_DIR}/src/memmap_custom_rp2040.ld)
  endif()
endif()


# eof :-)
