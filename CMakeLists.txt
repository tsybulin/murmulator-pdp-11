cmake_minimum_required(VERSION 3.22)
include(pico_sdk_import.cmake)

project(murm-pdp C CXX ASM)
pico_sdk_init()

option(VGA "Enable VGA" ON)
set(APP_VERSION 11.04)

SET(BUILD_NAME "${PROJECT_NAME}-${APP_VERSION}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

set(FAMILY rp2040)
set(BOARD pico_sdk)


add_subdirectory(drivers/vga)
add_subdirectory(drivers/graphics)
add_subdirectory(drivers/sdcard)
add_subdirectory(drivers/fatfs)
add_subdirectory(drivers/ps2)

add_subdirectory(cons)
add_subdirectory(pdp11)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

message(STATUS "Add source files:")
foreach (SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach ()
message(STATUS "")

add_executable(${PROJECT_NAME} ${SRC})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

pico_set_program_name(${PROJECT_NAME} "Murmulator PDP11")
pico_set_program_version(${PROJECT_NAME} "test")

target_link_libraries(${PROJECT_NAME} PRIVATE graphics sdcard fatfs ps2 cons pdp11 pico_stdlib hardware_pio pico_multicore)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        # VGA 8 pins starts from pin:
        VGA_BASE_PIN=6

        # SDCARD
        SDCARD_PIN_SPI0_SCK=2
        SDCARD_PIN_SPI0_MOSI=3
        SDCARD_PIN_SPI0_MISO=4
        SDCARD_PIN_SPI0_CS=5

        # PS2 keyboard
        KBD_CLOCK_PIN=0
        KBD_DATA_PIN=1

        # Serial / DL1
        PIN_UART_ID=uart0
        PIN_UART_TX=16
        PIN_UART_RX=17
        PIN_UART_CTS=18
        PIN_UART_RTS=19
)

target_compile_definitions(${PROJECT_NAME} PRIVATE VGA)
target_link_libraries(${PROJECT_NAME} PRIVATE vga)
SET(BUILD_NAME "${BUILD_NAME}-VGA")

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")

pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_enable_stdio_usb(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})
