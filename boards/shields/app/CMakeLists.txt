# Temperature sensor behavior
if(CONFIG_ZMK_TEMP_SENSOR)
    target_sources(app PRIVATE src/behaviors/behavior_temp_sensor.c)
    target_sources(app PRIVATE src/events/temp_sensor_changed.c)
    target_sources(app PRIVATE src/ble/services/temp.c)
endif()