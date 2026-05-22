cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rs_nexus_ble_imu)

target_include_directories(app PRIVATE
    include
    src/ble
    src/imu
    src/streaming
    src/led
    src/util
)

target_sources(app PRIVATE
    src/main.c

    src/ble/ble_services.c
    src/ble/imu_measurement_service.c
    src/ble/device_control_service.c

    src/streaming/streamer.c

    src/imu/imu.c

    src/led/status_led.c

    src/util/timebase.c
)