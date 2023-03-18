# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/tree/downloaded_tools/pico/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
project(video_ram_replacement C CXX ASM)
set(PICO_COPY_TO_RAM 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# SD FAT LIB
add_subdirectory(/tree/downloaded_tools/pico/ajm-no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI build)

# Add executable. Default name is the project name, version 0.1

add_executable(video_ram_replacement
video_ram_replacement.c
)

#pico_generate_pio_header(video_ram_replacement ${CMAKE_CURRENT_LIST_DIR}/picoputer.pio)

pico_set_program_name(video_ram_replacement "video_ram_replacement")
pico_set_program_version(video_ram_replacement "0.1")

#pico_enable_stdio_uart(video_ram_replacement 0)
pico_enable_stdio_usb(video_ram_replacement 1)

# Add the standard library to the build
target_link_libraries(video_ram_replacement pico_stdlib)

IF (NOT DEFINED N_SD_CARDS)
    SET(N_SD_CARDS 1)
ENDIF()
target_compile_definitions(video_ram_replacement PUBLIC DEBUG N_SD_CARDS=${N_SD_CARDS})

# Add any user requested libraries
target_link_libraries(video_ram_replacement
        #hardware_i2c
        hardware_pio
        hardware_clocks
	hardware_adc
        pico_sd_card
        pico_multicore
	FatFs_SPI
        )

pico_add_extra_outputs(video_ram_replacement)

