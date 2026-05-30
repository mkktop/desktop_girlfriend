idf_component_register(
    SRCS "main.c"
         "modules/event/app_event.c"
         "modules/display/app_display.c"
         "modules/display/ui/ui_manager.c"
         "modules/display/ui/ui_home.c"
         "modules/wifi/app_wifi.c"
    INCLUDE_DIRS "." "modules/event" "modules/display" "modules/display/ui" "modules/wifi"
    EMBED_FILES "resources/html/index.html"
)
