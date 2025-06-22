// include/zmk/events/temp_sensor_changed.h
// Temperature sensor event header

#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

struct zmk_temp_sensor_changed {
    int16_t temperature; // Temperature in Celsius * 100
};

ZMK_EVENT_DECLARE(zmk_temp_sensor_changed);
