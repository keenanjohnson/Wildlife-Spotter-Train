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

// Pybricks BLE UUIDs
// Pybricks Service: c5f50001-8280-46da-89f4-6d8051e4aeef
// Command/Event Characteristic: c5f50002-8280-46da-89f4-6d8051e4aeef
// UUID bytes are stored in little-endian order
static const ble_uuid128_t pybricks_svc_uuid = BLE_UUID128_INIT(
    0xef, 0xae, 0xe4, 0x51, 0x80, 0x6d, 0xf4, 0x89,
    0xda, 0x46, 0x80, 0x82, 0x01, 0x00, 0xf5, 0xc5
);

static const ble_uuid128_t pybricks_cmd_chr_uuid = BLE_UUID128_INIT(
    0xef, 0xae, 0xe4, 0x51, 0x80, 0x6d, 0xf4, 0x89,
    0xda, 0x46, 0x80, 0x82, 0x02, 0x00, 0xf5, 0xc5
);

// Write stdin command prefix
#define PYBRICKS_CMD_WRITE_STDIN 0x06

// Connection state
typedef enum {
    TRAIN_BLE_DISCONNECTED = 0,
    TRAIN_BLE_SCANNING,
    TRAIN_BLE_CONNECTING,
    TRAIN_BLE_DISCOVERING,
    TRAIN_BLE_CONNECTED,
    TRAIN_BLE_READY
} train_ble_state_t;

static train_ble_state_t train_state = TRAIN_BLE_DISCONNECTED;
static uint16_t train_conn_handle = 0;
static uint16_t train_cmd_chr_handle = 0;
static ble_addr_t train_hub_addr;
static bool train_hub_found = false;
static SemaphoreHandle_t train_ble_mutex = NULL;

// Forward declarations
static void train_ble_scan_start(void);
static int train_ble_gap_event(struct ble_gap_event *event, void *arg);

// Get current connection state
static train_ble_state_t train_get_state(void) {
    return train_state;
}

// Get state as string for status reporting
static const char* train_state_str(void) {
    switch (train_state) {
        case TRAIN_BLE_DISCONNECTED: return "disconnected";
        case TRAIN_BLE_SCANNING: return "scanning";
        case TRAIN_BLE_CONNECTING: return "connecting";
        case TRAIN_BLE_DISCOVERING: return "discovering";
        case TRAIN_BLE_CONNECTED: return "connected";
        case TRAIN_BLE_READY: return "ready";
        default: return "unknown";
    }
}

// Discovery callback for finding the command characteristic
static int train_ble_on_disc_complete(uint16_t conn_handle,
                                       const struct ble_gatt_error *error,
                                       const struct ble_gatt_svc *service,
                                       void *arg) {
    if (error->status != 0) {
        ESP_LOGE(BLE_TAG, "Service discovery failed: %d", error->status);
        return 0;
    }
    ESP_LOGI(BLE_TAG, "Service discovery complete");
    return 0;
}

// Characteristic discovery callback
static int train_ble_on_chr_disc(uint16_t conn_handle,
                                  const struct ble_gatt_error *error,
                                  const struct ble_gatt_chr *chr,
                                  void *arg) {
    if (error->status == 0 && chr != NULL) {
        if (ble_uuid_cmp(&chr->uuid.u, &pybricks_cmd_chr_uuid.u) == 0) {
            train_cmd_chr_handle = chr->val_handle;
            ESP_LOGI(BLE_TAG, "Found Pybricks command characteristic, handle: %d", train_cmd_chr_handle);
            train_state = TRAIN_BLE_READY;
        }
    } else if (error->status == BLE_HS_EDONE) {
        if (train_cmd_chr_handle != 0) {
            ESP_LOGI(BLE_TAG, "Characteristic discovery complete, ready to send commands");
        } else {
            ESP_LOGE(BLE_TAG, "Command characteristic not found");
        }
    }
    return 0;
}

// Start service/characteristic discovery after connection
static void train_ble_discover_services(uint16_t conn_handle) {
    train_state = TRAIN_BLE_DISCOVERING;

    int rc = ble_gattc_disc_chrs_by_uuid(
        conn_handle,
        1, 0xFFFF,  // Search all handles
        &pybricks_cmd_chr_uuid.u,
        train_ble_on_chr_disc,
        NULL
    );

    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Failed to start characteristic discovery: %d", rc);
    }
}

// GAP event handler
static int train_ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            // Check if this is a Pybricks hub by looking for the service UUID in advertising data
            struct ble_hs_adv_fields fields;
            int rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);

            if (rc == 0) {
                // Debug: Log ALL discovered devices
                char addr_str[18];
                snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                    event->disc.addr.val[5], event->disc.addr.val[4],
                    event->disc.addr.val[3], event->disc.addr.val[2],
                    event->disc.addr.val[1], event->disc.addr.val[0]);

                char name[32] = {0};
                if (fields.name != NULL && fields.name_len > 0) {
                    size_t len = fields.name_len < 31 ? fields.name_len : 31;
                    memcpy(name, fields.name, len);
                }

                ESP_LOGI(BLE_TAG, "Discovered: %s name='%s' uuids128=%d rssi=%d",
                    addr_str, name[0] ? name : "(none)",
                    fields.num_uuids128, event->disc.rssi);

                // Check for Pybricks service UUID in the advertisement
                // Also check device name contains "Pybricks" or "City Hub"
                bool is_pybricks = false;

                if (name[0] != '\0') {
                    if (strstr(name, "Pybricks") != NULL ||
                        strstr(name, "City Hub") != NULL ||
                        strstr(name, "LEGO") != NULL ||
                        strcmp(name, "train") == 0) {
                        is_pybricks = true;
                        ESP_LOGI(BLE_TAG, "Found Pybricks hub: %s", name);
                    }
                }

                // Check for Pybricks service UUID in 128-bit UUIDs
                for (int i = 0; i < fields.num_uuids128; i++) {
                    if (ble_uuid_cmp(&fields.uuids128[i].u, &pybricks_svc_uuid.u) == 0) {
                        is_pybricks = true;
                        ESP_LOGI(BLE_TAG, "Found device advertising Pybricks service");
                        break;
                    }
                }

                if (is_pybricks && !train_hub_found) {
                    train_hub_found = true;
                    train_hub_addr = event->disc.addr;

                    ESP_LOGI(BLE_TAG, "Pybricks hub found! Stopping scan and connecting...");
                    ble_gap_disc_cancel();

                    train_state = TRAIN_BLE_CONNECTING;
                    rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &train_hub_addr, 10000, NULL,
                                        train_ble_gap_event, NULL);
                    if (rc != 0) {
                        ESP_LOGE(BLE_TAG, "Failed to connect: %d", rc);
                        train_hub_found = false;
                        train_state = TRAIN_BLE_DISCONNECTED;
                    }
                }
            }
            break;
        }

        case BLE_GAP_EVENT_DISC_COMPLETE:
            ESP_LOGI(BLE_TAG, "Scan complete");
            if (!train_hub_found) {
                ESP_LOGI(BLE_TAG, "Hub not found, restarting scan in 5s...");
                train_state = TRAIN_BLE_DISCONNECTED;
                vTaskDelay(pdMS_TO_TICKS(5000));
                train_ble_scan_start();
            }
            break;

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(BLE_TAG, "Connected to hub!");
                train_conn_handle = event->connect.conn_handle;
                train_state = TRAIN_BLE_CONNECTED;

                // Start service discovery
                train_ble_discover_services(train_conn_handle);
            } else {
                ESP_LOGE(BLE_TAG, "Connection failed: %d", event->connect.status);
                train_hub_found = false;
                train_state = TRAIN_BLE_DISCONNECTED;
                vTaskDelay(pdMS_TO_TICKS(2000));
                train_ble_scan_start();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(BLE_TAG, "Disconnected from hub, reason: %d", event->disconnect.reason);
            train_conn_handle = 0;
            train_cmd_chr_handle = 0;
            train_hub_found = false;
            train_state = TRAIN_BLE_DISCONNECTED;

            // Auto-reconnect after delay
            vTaskDelay(pdMS_TO_TICKS(2000));
            train_ble_scan_start();
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(BLE_TAG, "MTU updated: %d", event->mtu.value);
            break;

        default:
            break;
    }

    return 0;
}

// Start BLE scanning
static void train_ble_scan_start(void) {
    if (train_state == TRAIN_BLE_SCANNING || train_state >= TRAIN_BLE_CONNECTING) {
        return;
    }

    train_state = TRAIN_BLE_SCANNING;
    train_hub_found = false;

    struct ble_gap_disc_params disc_params = {
        .itvl = 160,           // 100ms interval
        .window = 80,          // 50ms window
        .filter_policy = 0,
        .limited = 0,
        .passive = 0,          // Active scanning
        .filter_duplicates = 1,
    };

    ESP_LOGI(BLE_TAG, "Starting BLE scan for Pybricks hub...");

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30000, &disc_params,
                          train_ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Failed to start scan: %d", rc);
        train_state = TRAIN_BLE_DISCONNECTED;
    }
}

// Send command to train
static int train_send_command(const char *cmd) {
    if (train_state != TRAIN_BLE_READY || train_cmd_chr_handle == 0) {
        ESP_LOGW(BLE_TAG, "Cannot send command, not ready (state: %s)", train_state_str());
        return -1;
    }

    // Build command packet: [0x06][command][newline]
    size_t cmd_len = strlen(cmd);
    uint8_t buf[32];
    buf[0] = PYBRICKS_CMD_WRITE_STDIN;
    memcpy(&buf[1], cmd, cmd_len);
    buf[1 + cmd_len] = '\n';
    size_t total_len = 2 + cmd_len;

    ESP_LOGI(BLE_TAG, "Sending command: %s", cmd);

    int rc = ble_gattc_write_no_rsp_flat(train_conn_handle, train_cmd_chr_handle, buf, total_len);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Failed to send command: %d", rc);
        return -1;
    }

    return 0;
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
    train_ble_scan_start();
}

// Initialize BLE for train control
static void train_ble_init(void) {
    ESP_LOGI(BLE_TAG, "Initializing train BLE...");

    train_ble_mutex = xSemaphoreCreateMutex();

    // Initialize NimBLE
    int rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Failed to init NimBLE port: %d", rc);
        return;
    }

    // Configure the host
    ble_hs_cfg.sync_cb = train_ble_on_sync;

    // Note: We don't need to set device name since we're a BLE central only (not advertising)

    // Start the host task
    nimble_port_freertos_init(train_ble_host_task);

    ESP_LOGI(BLE_TAG, "Train BLE initialized");
}
