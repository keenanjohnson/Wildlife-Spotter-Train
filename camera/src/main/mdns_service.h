#pragma once

#include <mdns.h>
#include <esp_log.h>

#define MDNS_HOSTNAME "train"
#define MDNS_INSTANCE "Wildlife Spotter Train Camera"

static const char *MDNS_TAG = "MDNS";

static void start_mdns_service(void) {
    // Initialize mDNS
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(MDNS_TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return;
    }

    // Set hostname (will be accessible as wildlife-train.local)
    err = mdns_hostname_set(MDNS_HOSTNAME);
    if (err != ESP_OK) {
        ESP_LOGE(MDNS_TAG, "mDNS hostname set failed: %s", esp_err_to_name(err));
        return;
    }

    // Set instance name (friendly name for service browsers)
    err = mdns_instance_name_set(MDNS_INSTANCE);
    if (err != ESP_OK) {
        ESP_LOGE(MDNS_TAG, "mDNS instance name set failed: %s", esp_err_to_name(err));
        return;
    }

    // Advertise HTTP service on port 80 (API server)
    err = mdns_service_add(MDNS_INSTANCE, "_http", "_tcp", 80, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(MDNS_TAG, "mDNS service add (port 80) failed: %s", esp_err_to_name(err));
    }

    // Advertise HTTP service on port 81 (stream server)
    mdns_service_add("Wildlife Train Stream", "_http", "_tcp", 81, NULL, 0);

    ESP_LOGI(MDNS_TAG, "mDNS service started");
    ESP_LOGI(MDNS_TAG, "  Hostname: %s.local", MDNS_HOSTNAME);
    ESP_LOGI(MDNS_TAG, "  Web UI:   http://%s.local/", MDNS_HOSTNAME);
    ESP_LOGI(MDNS_TAG, "  Stream:   http://%s.local:81/stream", MDNS_HOSTNAME);
}
