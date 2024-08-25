add_library(vga INTERFACE)

target_sources(vga INTERFACE ${CMAKE_CURRENT_LIST_DIR}/vga.c)
target_link_libraries(vga INTERFACE hardware_pio hardware_dma)
target_include_directories(vga INTERFACE ${CMAKE_CURRENT_LIST_DIR})
