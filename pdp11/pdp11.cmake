if (NOT TARGET pdp11)
    add_library(pdp11 INTERFACE)

    target_sources(pdp11 INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/avr11.cxx
        ${CMAKE_CURRENT_LIST_DIR}/disasm.cxx
#       ${CMAKE_CURRENT_LIST_DIR}/getline.cxx
        ${CMAKE_CURRENT_LIST_DIR}/kb11.cxx
        ${CMAKE_CURRENT_LIST_DIR}/kl11.cxx
        ${CMAKE_CURRENT_LIST_DIR}/kt11.cxx
        ${CMAKE_CURRENT_LIST_DIR}/kw11.cxx
        ${CMAKE_CURRENT_LIST_DIR}/lp11.cxx
        ${CMAKE_CURRENT_LIST_DIR}/pc11.cxx
#       ${CMAKE_CURRENT_LIST_DIR}/Pico_1140.cxx
        ${CMAKE_CURRENT_LIST_DIR}/rk11.cxx
        ${CMAKE_CURRENT_LIST_DIR}/rl11.cxx
        ${CMAKE_CURRENT_LIST_DIR}/unibus.cxx
        ${CMAKE_CURRENT_LIST_DIR}/dl11.cxx
        ${CMAKE_CURRENT_LIST_DIR}/fp11.cxx
#       ${CMAKE_CURRENT_LIST_DIR}/hw_config.c
    )

    target_link_libraries(pdp11 INTERFACE pico_stdlib)
    target_include_directories(pdp11 INTERFACE ${CMAKE_CURRENT_LIST_DIR})
endif ()
