add_library(ps2 INTERFACE)

target_sources(ps2 INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/ps2.c
        ${CMAKE_CURRENT_LIST_DIR}/ps2.h
        ${CMAKE_CURRENT_LIST_DIR}/hid.h
)

target_link_libraries(ps2 INTERFACE hardware_pio hardware_clocks)

target_include_directories(ps2 INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)
