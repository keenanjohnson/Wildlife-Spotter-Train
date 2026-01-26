#pragma once

#include <string.h>
#include <esp_log.h>
#include <esp_bt.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static const char *BLE_TAG = "TRAIN_BLE";

// Pybricks Service UUID: c5f50001-8280-46da-89f4-6d8051e4aeef (little-endian)
static const ble_uuid128_t pybricks_svc_uuid = BLE_UUID128_INIT(
    0xef, 0xae, 0xe4, 0x51, 0x80, 0x6d, 0xf4, 0x89,
    0xda, 0x46, 0x80, 0x82, 0x01, 0x00, 0xf5, 0xc5
);

// Pybricks Characteristic UUID: c5f50002-8280-46da-89f4-6d8051e4aeef (little-endian)
static const ble_uuid128_t pybricks_chr_uuid = BLE_UUID128_INIT(
    0xef, 0xae, 0xe4, 0x51, 0x80, 0x6d, 0xf4, 0x89,
    0xda, 0x46, 0x80, 0x82, 0x02, 0x00, 0xf5, 0xc5
);

// Connection state
typedef enum {
    TRAIN_BLE_DISCONNECTED = 0,
    TRAIN_BLE_SCANNING,
    TRAIN_BLE_CONNECTING,
    TRAIN_BLE_DISCOVERING,
    TRAIN_BLE_READY
} train_ble_state_t;

static train_ble_state_t train_state = TRAIN_BLE_DISCONNECTED;
static uint16_t train_conn_handle = 0;
static uint16_t train_chr_val_handle = 0;
static SemaphoreHandle_t train_ble_mutex = NULL;

// Pending command
static char pending_command[8] = {0};
static bool has_pending_command = false;

// Forward declarations
static void train_ble_scan_start(void);
static int train_ble_gap_event(struct ble_gap_event *event, void *arg);

// Get state as string for status reporting
static const char* train_state_str(void) {
    switch (train_state) {
        case TRAIN_BLE_DISCONNECTED: return "disconnected";
        case TRAIN_BLE_SCANNING: return "scanning";
        case TRAIN_BLE_CONNECTING: return "connecting";
        case TRAIN_BLE_DISCOVERING: return "discovering";
        case TRAIN_BLE_READY: return "ready";
        default: return "unknown";
    }
}

// Write command to Pybricks stdin characteristic
// Format: [0x06][command string][newline]
static int train_write_command(const char *cmd) {
    if (train_state != TRAIN_BLE_READY || train_chr_val_handle == 0) {
        ESP_LOGW(BLE_TAG, "Cannot write: not connected (state=%s, handle=%d)",
            train_state_str(), train_chr_val_handle);
        return -1;
    }

    // Build stdin write packet: 0x06 prefix + command + newline
    uint8_t buf[32];
    size_t cmd_len = strlen(cmd);
    if (cmd_len > sizeof(buf) - 3) {
        cmd_len = sizeof(buf) - 3;
    }

    buf[0] = 0x06;  // Pybricks stdin write command
    memcpy(&buf[1], cmd, cmd_len);
    buf[1 + cmd_len] = '\n';
    size_t total_len = 2 + cmd_len;

    ESP_LOGI(BLE_TAG, "Writing to characteristic: %.*s", (int)cmd_len, cmd);

    int rc = ble_gattc_write_flat(train_conn_handle, train_chr_val_handle,
                                   buf, total_len, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Write failed: %d", rc);
        return -1;
    }

    return 0;
}

// Service discovery callback
static int train_on_disc_complete(uint16_t conn_handle,
                                   const struct ble_gatt_error *error,
                                   const struct ble_gatt_svc *service,
                                   void *arg) {
    if (error->status == 0) {
        ESP_LOGI(BLE_TAG, "Service found! Start handle: %d, End handle: %d",
            service->start_handle, service->end_handle);
    } else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(BLE_TAG, "Service discovery complete");
    } else {
        ESP_LOGE(BLE_TAG, "Service discovery error: %d", error->status);
    }
    return 0;
}

// Characteristic discovery callback
static int train_on_chr_disc_complete(uint16_t conn_handle,
                                       const struct ble_gatt_error *error,
                                       const struct ble_gatt_chr *chr,
                                       void *arg) {
    if (error->status == 0 && chr != NULL) {
        char uuid_str[BLE_UUID_STR_LEN];
        ble_uuid_to_str(&chr->uuid.u, uuid_str);
        ESP_LOGI(BLE_TAG, "Characteristic found: %s, val_handle: %d, def_handle: %d",
            uuid_str, chr->val_handle, chr->def_handle);

        // Check if this is the Pybricks characteristic
        if (ble_uuid_cmp(&chr->uuid.u, &pybricks_chr_uuid.u) == 0) {
            train_chr_val_handle = chr->val_handle;
            ESP_LOGI(BLE_TAG, "Found Pybricks characteristic! Handle: %d", train_chr_val_handle);
        }
    } else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(BLE_TAG, "Characteristic discovery complete");

        if (train_chr_val_handle != 0) {
            train_state = TRAIN_BLE_READY;
            ESP_LOGI(BLE_TAG, "Train BLE READY! Can send commands now.");

            // Send any pending command
            if (has_pending_command) {
                has_pending_command = false;
                train_write_command(pending_command);
            }
        } else {
            ESP_LOGE(BLE_TAG, "Pybricks characteristic not found!");
            ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
    } else {
        ESP_LOGE(BLE_TAG, "Characteristic discovery error: %d", error->status);
    }
    return 0;
}

// Start characteristic discovery
static void train_discover_characteristics(uint16_t conn_handle) {
    train_state = TRAIN_BLE_DISCOVERING;
    ESP_LOGI(BLE_TAG, "Starting characteristic discovery...");

    int rc = ble_gattc_disc_chrs_by_uuid(conn_handle, 1, 0xFFFF,
                                          &pybricks_chr_uuid.u,
                                          train_on_chr_disc_complete, NULL);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Failed to start characteristic discovery: %d", rc);
    }
}

// GAP event handler
static int train_ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            // Check for Pybricks service in advertising data
            struct ble_hs_adv_fields fields;
            int rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                              event->disc.length_data);
            if (rc != 0) {
                break;
            }

            // Check if this device has the Pybricks service UUID
            bool has_pybricks = false;

            // Check 128-bit UUIDs
            if (fields.uuids128 != NULL) {
                for (int i = 0; i < fields.num_uuids128; i++) {
                    if (ble_uuid_cmp(&fields.uuids128[i].u, &pybricks_svc_uuid.u) == 0) {
                        has_pybricks = true;
                        break;
                    }
                }
            }

            // Also check device name for "Pybricks" or known hub names
            if (!has_pybricks && fields.name != NULL && fields.name_len > 0) {
                char name[32] = {0};
                int len = fields.name_len < 31 ? fields.name_len : 31;
                memcpy(name, fields.name, len);

                if (strstr(name, "Pybricks") != NULL ||
                    strstr(name, "train") != NULL ||
                    strstr(name, "City") != NULL ||
                    strstr(name, "Hub") != NULL) {
                    has_pybricks = true;
                    ESP_LOGI(BLE_TAG, "Found hub by name: %s", name);
                }
            }

            if (has_pybricks) {
                ESP_LOGI(BLE_TAG, "Found Pybricks hub! Connecting...");

                // Stop scanning
                ble_gap_disc_cancel();

                // Connect to the device
                train_state = TRAIN_BLE_CONNECTING;
                rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr,
                                      10000, NULL, train_ble_gap_event, NULL);
                if (rc != 0) {
                    ESP_LOGE(BLE_TAG, "Failed to connect: %d", rc);
                    train_state = TRAIN_BLE_DISCONNECTED;
                    train_ble_scan_start();
                }
            }
            break;
        }

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(BLE_TAG, "Connected! conn_handle=%d", event->connect.conn_handle);
                train_conn_handle = event->connect.conn_handle;
                train_chr_val_handle = 0;

                // Start characteristic discovery
                train_discover_characteristics(train_conn_handle);
            } else {
                ESP_LOGE(BLE_TAG, "Connection failed: %d", event->connect.status);
                train_state = TRAIN_BLE_DISCONNECTED;
                train_conn_handle = 0;
                train_ble_scan_start();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(BLE_TAG, "Disconnected, reason=%d", event->disconnect.reason);
            train_state = TRAIN_BLE_DISCONNECTED;
            train_conn_handle = 0;
            train_chr_val_handle = 0;

            // Restart scanning
            vTaskDelay(pdMS_TO_TICKS(1000));
            train_ble_scan_start();
            break;

        case BLE_GAP_EVENT_DISC_COMPLETE:
            ESP_LOGI(BLE_TAG, "Scan complete, restarting...");
            if (train_state == TRAIN_BLE_SCANNING) {
                train_state = TRAIN_BLE_DISCONNECTED;
                vTaskDelay(pdMS_TO_TICKS(500));
                train_ble_scan_start();
            }
            break;

        default:
            break;
    }

    return 0;
}

// Start BLE scanning
static void train_ble_scan_start(void) {
    if (train_state != TRAIN_BLE_DISCONNECTED) {
        ESP_LOGW(BLE_TAG, "Cannot start scan in state: %s", train_state_str());
        return;
    }

    train_state = TRAIN_BLE_SCANNING;

    struct ble_gap_disc_params disc_params = {
        .itvl = 160,            // 100ms
        .window = 80,           // 50ms
        .filter_policy = 0,
        .limited = 0,
        .passive = 0,           // Active scanning
        .filter_duplicates = 1,
    };

    ESP_LOGI(BLE_TAG, "Starting BLE scan for Pybricks hub...");

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 10000, &disc_params,
                          train_ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Failed to start scan: %d", rc);
        train_state = TRAIN_BLE_DISCONNECTED;
    }
}

// Send command to train (public API)
static int train_send_command(const char *cmd) {
    ESP_LOGI(BLE_TAG, "train_send_command: %s (state=%s)", cmd, train_state_str());

    if (train_state == TRAIN_BLE_READY) {
        return train_write_command(cmd);
    } else {
        // Queue command for when connected
        strncpy(pending_command, cmd, sizeof(pending_command) - 1);
        has_pending_command = true;
        ESP_LOGW(BLE_TAG, "Queued command (not connected yet): %s", cmd);
        return 0;
    }
}

// NimBLE host task
static void train_ble_host_task(void *param) {
    ESP_LOGI(BLE_TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// Called when BLE stack is synced and ready
static void train_ble_on_sync(void) {
    ESP_LOGI(BLE_TAG, "BLE stack synced, starting scan...");
    train_state = TRAIN_BLE_DISCONNECTED;
    train_ble_scan_start();
}

// Initialize BLE for train control
static void train_ble_init(void) {
    ESP_LOGI(BLE_TAG, "Initializing train BLE (GATT client mode)...");

    train_ble_mutex = xSemaphoreCreateMutex();

    // Initialize NimBLE
    int rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Failed to init NimBLE port: %d", rc);
        return;
    }

    // Configure the host
    ble_hs_cfg.sync_cb = train_ble_on_sync;

    // Start the host task
    nimble_port_freertos_init(train_ble_host_task);

    ESP_LOGI(BLE_TAG, "Train BLE initialized (GATT client mode)");
}
