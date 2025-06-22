// src/behaviors/behavior_temp_sensor.c
// ZMK Temperature Sensor Behavior

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/temp_sensor_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_temp_sensor_config {
    const struct device *sensor;
    uint32_t update_interval_ms;
};

struct behavior_temp_sensor_data {
    struct k_work_delayable update_work;
    int16_t last_temperature;
};

static void temp_sensor_update_work_handler(struct k_work *work) {
    struct k_work_delayable *delayable_work = k_work_delayable_from_work(work);
    struct behavior_temp_sensor_data *data = 
        CONTAINER_OF(delayable_work, struct behavior_temp_sensor_data, update_work);
    
    const struct device *dev = DEVICE_DT_INST_GET(0);
    const struct behavior_temp_sensor_config *config = dev->config;
    
    struct sensor_value temp_val;
    int err;
    
    if (!device_is_ready(config->sensor)) {
        LOG_ERR("Temperature sensor not ready");
        goto reschedule;
    }
    
    err = sensor_sample_fetch(config->sensor);
    if (err) {
        LOG_ERR("Failed to fetch temperature sample: %d", err);
        goto reschedule;
    }
    
    err = sensor_channel_get(config->sensor, SENSOR_CHAN_DIE_TEMP, &temp_val);
    if (err) {
        LOG_ERR("Failed to get temperature: %d", err);
        goto reschedule;
    }
    
    // Convert to celsius * 100 for precision
    int16_t temp_celsius_x100 = (int16_t)(sensor_value_to_double(&temp_val) * 100);
    
    // Only send event if temperature changed significantly (> 0.5°C)
    if (abs(temp_celsius_x100 - data->last_temperature) > 50) {
        data->last_temperature = temp_celsius_x100;
        
        LOG_INF("Temperature: %d.%02d°C", 
                temp_celsius_x100 / 100, 
                abs(temp_celsius_x100 % 100));
        
        // Raise temperature changed event
        raise_zmk_temp_sensor_changed(
            (struct zmk_temp_sensor_changed) { .temperature = temp_celsius_x100 }
        );
    }
    
reschedule:
    k_work_schedule(&data->update_work, K_MSEC(config->update_interval_ms));
}

static int behavior_temp_sensor_init(const struct device *dev) {
    struct behavior_temp_sensor_data *data = dev->data;
    const struct behavior_temp_sensor_config *config = dev->config;
    
    if (!device_is_ready(config->sensor)) {
        LOG_ERR("Temperature sensor device not ready");
        return -ENODEV;
    }
    
    k_work_init_delayable(&data->update_work, temp_sensor_update_work_handler);
    
    // Start temperature monitoring
    k_work_schedule(&data->update_work, K_MSEC(1000));
    
    LOG_INF("Temperature sensor behavior initialized");
    return 0;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
    // This behavior doesn't respond to key presses
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    // This behavior doesn't respond to key releases
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_temp_sensor_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define TEMP_SENSOR_INST(n)                                                           \
    static struct behavior_temp_sensor_data behavior_temp_sensor_data_##n = {};       \
    static const struct behavior_temp_sensor_config behavior_temp_sensor_config_##n = { \
        .sensor = DEVICE_DT_GET(DT_INST_PHANDLE(n, sensor)),                         \
        .update_interval_ms = DT_INST_PROP(n, update_interval_ms),                   \
    };                                                                                \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_temp_sensor_init, NULL,                     \
                           &behavior_temp_sensor_data_##n,                           \
                           &behavior_temp_sensor_config_##n, POST_KERNEL,            \
                           CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                      \
                           &behavior_temp_sensor_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TEMP_SENSOR_INST)

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)