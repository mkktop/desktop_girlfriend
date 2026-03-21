idf_component_register(
    SRCS "main.c" "app_lvgl.c" "bsp/bsp_wifi_web.c"
    INCLUDE_DIRS "." "bsp"
    EMBED_FILES "html/index.html"
)
