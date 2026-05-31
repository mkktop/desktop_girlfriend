# 板卡选择：根据 Kconfig 选择的板卡编译对应目录的 board.c
if(CONFIG_BOARD_TYPE_DESKTOP_GIRLFRIEND_V1)
    set(BOARD_DIR "desktop_girlfriend_V1")
endif()

idf_component_register(
    SRCS "main.c"
         "boards/${BOARD_DIR}/board.c"
         "modules/event/app_event.c"
         "modules/display/app_display.c"
         "modules/display/app_font.c"
         "modules/display/gif/gifdec.c"
         "modules/display/gif/app_gif.c"
         "modules/display/ui/ui_manager.c"
         "modules/display/ui/ui_home.c"
         "modules/display/ui/ui_wifi_config.c"
         "modules/wifi/app_wifi.c"
         "modules/sntp/app_sntp.c"
    INCLUDE_DIRS "." "boards" "modules/event" "modules/display" "modules/display/gif" "modules/display/ui" "modules/wifi" "modules/sntp"
    EMBED_FILES "resources/html/index.html"
    REQUIRES esp_partition esp_psram esp_wifi nvs_flash esp_http_server esp_netif esp_timer json esp_event
)

# ====== 资源分区打包（esp_mmap_assets） ======
# 从 xiaozhi-fonts 组件复制资源到 staging 目录，打包到 assets 分区
idf_component_get_property(XIAOZHI_FONTS_DIR 78__xiaozhi-fonts COMPONENT_DIR)
set(ASSETS_STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets_staging")
file(MAKE_DIRECTORY ${ASSETS_STAGING_DIR})

# CBin 字体文件
file(COPY "${XIAOZHI_FONTS_DIR}/cbin/font_puhui_common_16_4.bin"
     DESTINATION ${ASSETS_STAGING_DIR})

# 64x64 GIF 动态表情（21 个，~1.6MB）
file(GLOB EMOJI_GIFS "${XIAOZHI_FONTS_DIR}/gif/noto-emoji_64/*.gif")
file(COPY ${EMOJI_GIFS} DESTINATION ${ASSETS_STAGING_DIR})

# 自动打包到 assets 分区，随 idf.py flash 一起烧录
spiffs_create_partition_assets(assets ${ASSETS_STAGING_DIR} FLASH_IN_PROJECT MMAP_FILE_SUPPORT_FORMAT ".bin,.gif")
