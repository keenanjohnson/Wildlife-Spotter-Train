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
    TRAIN_BLE_INITIALIZING,
    TRAIN_BLE_READY
} train_ble_state_t;

static train_ble_state_t train_state = TRAIN_BLE_DISCONNECTED;
static uint16_t train_conn_handle = 0;
static uint16_t train_chr_val_handle = 0;
static uint16_t train_cccd_handle = 0;
static bool motor_initialized = false;
static volatile bool repl_prompt_received = false;

// Forward declarations
static void train_ble_scan_start(void);
static int train_ble_gap_event(struct ble_gap_event *event, void *arg);
static void train_init_task(void *param);
static void train_subscribe_notifications(uint16_t conn_handle);

// Get state as string
static const char* train_state_str(void) {
    switch (train_state) {
        case TRAIN_BLE_DISCONNECTED: return "disconnected";
        case TRAIN_BLE_SCANNING: return "scanning";
        case TRAIN_BLE_CONNECTING: return "connecting";
        case TRAIN_BLE_DISCOVERING: return "discovering";
        case TRAIN_BLE_INITIALIZING: return "initializing";
        case TRAIN_BLE_READY: return "ready";
        default: return "unknown";
    }
}

// Write with acknowledgment callback
static volatile bool write_complete = false;
static int write_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                    struct ble_gatt_attr *attr, void *arg) {
    if (error->status != 0) {
        ESP_LOGE(BLE_TAG, "Write callback error: %d", error->status);
    }
    write_complete = true;
    return 0;
}

// Send data and wait for completion
static int train_write_wait(const uint8_t *data, size_t len) {
    if (train_chr_val_handle == 0 || train_conn_handle == 0) {
        ESP_LOGW(BLE_TAG, "Cannot write: not connected");
        return -1;
    }

    write_complete = false;

    int rc = ble_gattc_write_flat(train_conn_handle, train_chr_val_handle,
                                   data, len, write_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Write failed to initiate: %d", rc);
        return -1;
    }

    // Wait for write to complete (with longer timeout for WiFi/BLE coex)
    int timeout = 300;  // 3 seconds
    while (!write_complete && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
        timeout--;
    }

    if (!write_complete) {
        ESP_LOGW(BLE_TAG, "Write timeout");
        return -1;
    }

    return 0;
}

// Wait for REPL prompt with timeout
static bool train_wait_for_prompt(int timeout_ms) {
    int waited = 0;
    while (!repl_prompt_received && waited < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(50));
        waited += 50;
    }
    bool got_it = repl_prompt_received;
    repl_prompt_received = false;  // Reset for next command
    return got_it;
}

// Send stdin data to running program (single char, no line ending)
static int train_send_stdin(const char *data) {
    if (train_chr_val_handle == 0 || train_conn_handle == 0) {
        ESP_LOGW(BLE_TAG, "Cannot write stdin: not connected");
        return -1;
    }

    size_t len = strlen(data);
    uint8_t buf[128];

    if (len > sizeof(buf) - 1) {
        len = sizeof(buf) - 1;
    }

    buf[0] = 0x06;  // PBIO_PYBRICKS_COMMAND_WRITE_STDIN (0x05 is update mode!)
    memcpy(&buf[1], data, len);
    // No line ending needed - program reads single chars with stdin.read(1)

    ESP_LOGI(BLE_TAG, "Sending stdin (%d bytes): [0x06] + '%s'", len + 1, data);

    // Use write-with-response for reliability with WiFi/BLE coexistence
    return train_write_wait(buf, len + 1);
}

// Flag for program ready signal
static volatile bool program_ready_received = false;

// Initialize by starting pre-installed user program (runs in separate task)
static void train_init_task(void *param) {
    ESP_LOGI(BLE_TAG, "Starting train program...");

    // Start user program (must be pre-installed with: pybricksdev install ble train/main.py)
    ESP_LOGI(BLE_TAG, "Sending start program command (0x01)...");
    program_ready_received = false;
    uint8_t start_program = 0x01;
    if (train_write_wait(&start_program, 1) != 0) {
        ESP_LOGE(BLE_TAG, "Failed to start program");
        train_state = TRAIN_BLE_DISCONNECTED;
        vTaskDelete(NULL);
        return;
    }

    // Wait for program to signal ready (it prints "RDY")
    ESP_LOGI(BLE_TAG, "Waiting for program ready signal...");
    int waited = 0;
    while (!program_ready_received && waited < 5000) {
        vTaskDelay(pdMS_TO_TICKS(100));
        waited += 100;
    }

    if (program_ready_received) {
        ESP_LOGI(BLE_TAG, "Program ready!");
    } else {
        ESP_LOGW(BLE_TAG, "No ready signal, but continuing...");
    }

    motor_initialized = true;
    train_state = TRAIN_BLE_READY;
    ESP_LOGI(BLE_TAG, "Train BLE READY - use F/B/S commands");

    vTaskDelete(NULL);
}

// Send motor command (single character: F=forward, B=backward, S=stop)
static int train_write_command(const char *cmd) {
    if (train_state != TRAIN_BLE_READY || !motor_initialized) {
        ESP_LOGW(BLE_TAG, "Cannot write: not ready (state=%s, motor=%d)",
            train_state_str(), motor_initialized);
        return -1;
    }

    // Send single character command to the running program
    if (strcmp(cmd, "F") != 0 && strcmp(cmd, "B") != 0 && strcmp(cmd, "S") != 0) {
        ESP_LOGW(BLE_TAG, "Unknown command: %s (use F/B/S)", cmd);
        return -1;
    }

    return train_send_stdin(cmd);
}

// Subscribe to notifications callback
static int train_subscribe_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                               struct ble_gatt_attr *attr, void *arg) {
    if (error->status == 0) {
        ESP_LOGI(BLE_TAG, "Subscribed to notifications");
        // Now start motor initialization
        train_state = TRAIN_BLE_INITIALIZING;
        motor_initialized = false;
        xTaskCreate(train_init_task, "train_init", 4096, NULL, 5, NULL);
    } else {
        ESP_LOGE(BLE_TAG, "Subscribe failed: %d", error->status);
    }
    return 0;
}

// Subscribe to notifications
static void train_subscribe_notifications(uint16_t conn_handle) {
    // CCCD handle is usually val_handle + 1 for characteristics with notify property
    train_cccd_handle = train_chr_val_handle + 1;

    uint8_t value[2] = {0x01, 0x00};  // Enable notifications (0x0001)

    ESP_LOGI(BLE_TAG, "Subscribing to notifications (CCCD handle: %d)...", train_cccd_handle);
    int rc = ble_gattc_write_flat(conn_handle, train_cccd_handle, value, sizeof(value),
                                   train_subscribe_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Failed to subscribe: %d", rc);
        // Still try to initialize without notifications
        train_state = TRAIN_BLE_INITIALIZING;
        motor_initialized = false;
        xTaskCreate(train_init_task, "train_init", 4096, NULL, 5, NULL);
    }
}

// Characteristic discovery callback
static int train_on_chr_disc_complete(uint16_t conn_handle,
                                       const struct ble_gatt_error *error,
                                       const struct ble_gatt_chr *chr,
                                       void *arg) {
    if (error->status == 0 && chr != NULL) {
        if (ble_uuid_cmp(&chr->uuid.u, &pybricks_chr_uuid.u) == 0) {
            train_chr_val_handle = chr->val_handle;
            ESP_LOGI(BLE_TAG, "Found Pybricks characteristic! Handle: %d", train_chr_val_handle);
        }
    } else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(BLE_TAG, "Characteristic discovery complete");

        if (train_chr_val_handle != 0) {
            // Subscribe to notifications first, then initialize
            train_subscribe_notifications(conn_handle);
        } else {
            ESP_LOGE(BLE_TAG, "Pybricks characteristic not found!");
            ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
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
            struct ble_hs_adv_fields fields;
            int rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                              event->disc.length_data);
            if (rc != 0) break;

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

            // Check device name
            if (!has_pybricks && fields.name != NULL && fields.name_len > 0) {
                char name[32] = {0};
                int len = fields.name_len < 31 ? fields.name_len : 31;
                memcpy(name, fields.name, len);

                if (strstr(name, "Pybricks") != NULL ||
                    strstr(name, "train") != NULL ||
                    strstr(name, "City") != NULL) {
                    has_pybricks = true;
                    ESP_LOGI(BLE_TAG, "Found hub: %s", name);
                }
            }

            if (has_pybricks) {
                ESP_LOGI(BLE_TAG, "Found Pybricks hub! Connecting...");
                ble_gap_disc_cancel();
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
                ESP_LOGI(BLE_TAG, "Connected!");
                train_conn_handle = event->connect.conn_handle;
                train_chr_val_handle = 0;

                // Request larger MTU for longer messages (import statements can be 40+ bytes)
                int rc = ble_gattc_exchange_mtu(train_conn_handle, NULL, NULL);
                if (rc != 0) {
                    ESP_LOGW(BLE_TAG, "MTU exchange failed: %d", rc);
                }

                train_discover_characteristics(train_conn_handle);
            } else {
                ESP_LOGE(BLE_TAG, "Connection failed: %d", event->connect.status);
                train_state = TRAIN_BLE_DISCONNECTED;
                train_ble_scan_start();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(BLE_TAG, "Disconnected");
            train_state = TRAIN_BLE_DISCONNECTED;
            train_conn_handle = 0;
            train_chr_val_handle = 0;
            motor_initialized = false;
            vTaskDelay(pdMS_TO_TICKS(2000));
            train_ble_scan_start();
            break;

        case BLE_GAP_EVENT_DISC_COMPLETE:
            if (train_state == TRAIN_BLE_SCANNING) {
                train_state = TRAIN_BLE_DISCONNECTED;
                vTaskDelay(pdMS_TO_TICKS(500));
                train_ble_scan_start();
            }
            break;

        case BLE_GAP_EVENT_MTU: {
            ESP_LOGI(BLE_TAG, "MTU updated: %d", event->mtu.value);
            break;
        }

        case BLE_GAP_EVENT_NOTIFY_RX: {
            // Handle notifications from hub (stdout/stderr from REPL)
            struct os_mbuf *om = event->notify_rx.om;
            uint16_t len = OS_MBUF_PKTLEN(om);
            if (len > 0) {
                uint8_t buf[256];
                if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
                os_mbuf_copydata(om, 0, len, buf);
                buf[len] = '\0';

                // First byte is the event type (Pybricks protocol)
                // 0x00 = Status report, 0x01 = Stdout
                uint8_t event_type = buf[0];
                if (event_type == 0x00 && len > 1) {
                    // Status report - flags indicate hub state
                    // Bit 6 (0x40) = REPL running
                    uint8_t flags = buf[1];
                    ESP_LOGI(BLE_TAG, "Hub status: 0x%02x%s%s",
                        flags,
                        (flags & 0x40) ? " [REPL]" : "",
                        (flags & 0x02) ? " [PROG]" : "");
                } else if (event_type == 0x01 && len > 1) {
                    // Stdout data from program
                    ESP_LOGI(BLE_TAG, "Hub>>> %.*s", len - 1, &buf[1]);
                    // Check for program ready signal or command acknowledgments
                    if (strstr((char*)&buf[1], "RDY") != NULL) {
                        program_ready_received = true;
                        ESP_LOGI(BLE_TAG, "Program ready signal received!");
                    }
                } else {
                    ESP_LOGI(BLE_TAG, "Hub event: type=0x%02x len=%d", event_type, len);
                }
            }
            break;
        }

        default:
            break;
    }

    return 0;
}

// Start BLE scanning
static void train_ble_scan_start(void) {
    if (train_state != TRAIN_BLE_DISCONNECTED) return;

    train_state = TRAIN_BLE_SCANNING;

    struct ble_gap_disc_params disc_params = {
        .itvl = 160,
        .window = 80,
        .filter_policy = 0,
        .limited = 0,
        .passive = 0,
        .filter_duplicates = 1,
    };

    ESP_LOGI(BLE_TAG, "Scanning for Pybricks hub...");
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 10000, &disc_params,
                          train_ble_gap_event, NULL);
    if (rc != 0) {
        train_state = TRAIN_BLE_DISCONNECTED;
    }
}

// Public API: Send command to train
static int train_send_command(const char *cmd) {
    ESP_LOGI(BLE_TAG, "train_send_command: %s (state=%s)", cmd, train_state_str());
    return train_write_command(cmd);
}

// NimBLE host task
static void train_ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// Called when BLE stack is synced
static void train_ble_on_sync(void) {
    ESP_LOGI(BLE_TAG, "BLE ready, starting scan...");
    train_state = TRAIN_BLE_DISCONNECTED;
    train_ble_scan_start();
}

// Initialize BLE
static void train_ble_init(void) {
    ESP_LOGI(BLE_TAG, "Initializing train BLE...");

    int rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Failed to init NimBLE: %d", rc);
        return;
    }

    ble_hs_cfg.sync_cb = train_ble_on_sync;
    nimble_port_freertos_init(train_ble_host_task);

    ESP_LOGI(BLE_TAG, "Train BLE initialized");
}
