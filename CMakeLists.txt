cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rs_nexus_ar_hud)

target_include_directories(app PRIVATE
    include
    src/wifi
    src/vibration
    src/button
    src/display
    src/state_machine
    src/dummy
)

target_sources(app PRIVATE
    src/main.c
    src/wifi/wifi_manager.c
    src/vibration/vibration_manager.c
    src/button/button_manager.c
    src/display/display_manager.c
    src/state_machine/hud_state_machine.c
    src/dummy/dummy_nexus_client.c
)