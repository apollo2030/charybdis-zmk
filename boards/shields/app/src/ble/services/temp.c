#include <zephyr/types.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/temp_sensor_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Temperature Service UUID: 181A (Environmental Sensing Service)
#define BT_UUID_TEMP_SVC BT_UUID_ESS

// Temperature Characteristic UUID: 2A6E (Temperature)
#define BT_UUID_TEMP_CHAR BT_UUID_TEMPERATURE

static int16_t current_temperature = 0;
static bool notifications_enabled = false;

static ssize_t read_temperature(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset) {
    LOG_DBG("Temperature read: %d", current_temperature);
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           &current_temperature, sizeof(current_temperature));
}

static void temp_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    notifications_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Temperature notifications %s", 
            notifications_enabled ? "enabled" : "disabled");
}

// GATT service definition
BT_GATT_SERVICE_DEFINE(temp_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_TEMP_SVC),
    BT_GATT_CHARACTERISTIC(BT_UUID_TEMP_CHAR,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          read_temperature, NULL, NULL),
    BT_GATT_CCC(temp_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static int zmk_ble_temp_update(int16_t temperature) {
    current_temperature = temperature;
    
    if (!notifications_enabled) {
        return 0;
    }
    
    struct bt_conn *conn = zmk_ble_active_profile_conn();
    if (!conn) {
        return -ENOTCONN;
    }
    
    const struct bt_gatt_attr *attr = bt_gatt_find_by_uuid(temp_svc.attrs, 
                                                          temp_svc.attr_count,
                                                          BT_UUID_TEMP_CHAR);
    if (!attr) {
        LOG_ERR("Temperature characteristic not found");
        return -ENOENT;
    }
    
    int err = bt_gatt_notify(conn, attr, &current_temperature, 
                           sizeof(current_temperature));
    if (err) {
        LOG_ERR("Failed to notify temperature: %d", err);
    } else {
        LOG_DBG("Temperature notification sent: %d", temperature);
    }
    
    return err;
}

static int temp_sensor_changed_listener(const zmk_event_t *eh) {
    const struct zmk_temp_sensor_changed *ev = as_zmk_temp_sensor_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    zmk_ble_temp_update(ev->temperature);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(temp_sensor_listener, temp_sensor_changed_listener);
ZMK_SUBSCRIPTION(temp_sensor_listener, zmk_temp_sensor_changed);