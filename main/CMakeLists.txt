idf_component_register(
    SRCS "main.c" "modules/lvgl/app_lvgl.c" "modules/wifi/bsp_wifi_web.c" "modules/api/app_api.c"
    INCLUDE_DIRS "." "modules/lvgl" "modules/wifi" "modules/api"
    EMBED_FILES "resources/html/index.html"
)
