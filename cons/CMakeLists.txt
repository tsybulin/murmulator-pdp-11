add_library(cons INTERFACE)

target_sources(cons INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/cons.c
        ${CMAKE_CURRENT_LIST_DIR}/keyboard.c
        ${CMAKE_CURRENT_LIST_DIR}/cons.h
)

target_link_libraries(cons INTERFACE)

target_include_directories(cons INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)
