#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#include <esp_camera.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_http_server.h>

#include "camera.h"
#include "wifi_sta.h"
#include "web_ui.h"
#include "http_server.h"

static char const *const TAG = "CAMERA-MAIN";

void app_main(void) {
    // Early debug output
    printf("\n\n=== APP_MAIN STARTED ===\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Wildlife Spotter Train - MJPEG Streaming Server");
    ESP_LOGI(TAG, "Free heap at start: %lu bytes", (unsigned long)esp_get_free_heap_size());

    // Initialize WiFi station:
    ESP_LOGI(TAG, "Initializing WiFi...");
    wifi_init_sta();
    ESP_LOGI(TAG, "WiFi init complete. Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    // Wait for WiFi to connect before starting camera/server
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Initialize camera:
    ESP_LOGI(TAG, "Initializing camera...");
    init_camera();
    ESP_LOGI(TAG, "Camera init complete. Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    // Start HTTP server with MJPEG streaming:
    ESP_LOGI(TAG, "Starting HTTP server...");
    start_http_server();
    ESP_LOGI(TAG, "HTTP server started. Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    ESP_LOGI(TAG, "System ready!");

    // Main loop - just keep the task alive and log memory stats periodically
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000)); // Every 30 seconds

        ESP_LOGI(TAG, "Free heap: %lu bytes, min free: %lu bytes",
            (unsigned long)esp_get_free_heap_size(),
            (unsigned long)esp_get_minimum_free_heap_size());
    }
}
