set(LVGL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

file(GLOB_RECURSE LVGL_SOURCES "${LVGL_SRC_DIR}/*.c")

idf_component_register(SRCS ${LVGL_SOURCES}
                       "lv_port_disp.c"
                       INCLUDE_DIRS "." "src"
                       REQUIRES LCD)

target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Wno-unused-function
    -Wno-format
)
