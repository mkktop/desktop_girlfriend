idf_component_register(
    SRCS "main.c" "modules/lvgl/app_lvgl.c" "modules/wifi/bsp_wifi_web.c"
    INCLUDE_DIRS "." "modules/lvgl" "modules/wifi"
    EMBED_FILES "resources/html/index.html"
)
