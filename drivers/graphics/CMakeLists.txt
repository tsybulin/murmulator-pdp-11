add_library(graphics INTERFACE)

target_sources(graphics INTERFACE ${CMAKE_CURRENT_LIST_DIR}/graphics.c)
target_include_directories(graphics INTERFACE ${CMAKE_CURRENT_LIST_DIR})
