# 板卡选择：根据 Kconfig 选择的板卡编译对应目录的 board.c
if(CONFIG_BOARD_TYPE_DESKTOP_GIRLFRIEND_V1)
    set(BOARD_DIR "desktop_girlfriend_V1")
endif()

idf_component_register(
    SRCS "main.c"
         "boards/${BOARD_DIR}/board.c"
         "modules/event/app_event.c"
         "modules/display/app_display.c"
         "modules/display/ui/ui_manager.c"
         "modules/display/ui/ui_home.c"
         "modules/wifi/app_wifi.c"
    INCLUDE_DIRS "." "boards" "modules/event" "modules/display" "modules/display/ui" "modules/wifi"
    EMBED_FILES "resources/html/index.html"
)
