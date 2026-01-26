#pragma once

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define MJPEG_BOUNDARY "frame"
#define MJPEG_CONTENT_TYPE "multipart/x-mixed-replace;boundary=" MJPEG_BOUNDARY
#define MJPEG_PART_HEADER "Content-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n"
#define MJPEG_BOUNDARY_HEADER "\r\n--" MJPEG_BOUNDARY "\r\n"

static const char *HTTP_TAG = "HTTP";

// Forward declaration of web UI handler
static esp_err_t index_handler(httpd_req_t *req);

// MJPEG stream handler - streams frames continuously to the client
static esp_err_t stream_handler(httpd_req_t *req) {
    esp_err_t res = ESP_OK;
    char part_header[64];

    res = httpd_resp_set_type(req, MJPEG_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    // Disable buffering for real-time streaming
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");

    ESP_LOGI(HTTP_TAG, "MJPEG stream started");

    uint32_t frame_count = 0;
    int64_t last_log_time = esp_timer_get_time();

    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(HTTP_TAG, "Camera capture failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Send boundary
        res = httpd_resp_send_chunk(req, MJPEG_BOUNDARY_HEADER, strlen(MJPEG_BOUNDARY_HEADER));
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        // Send part header with content length
        size_t header_len = snprintf(part_header, sizeof(part_header), MJPEG_PART_HEADER, fb->len);
        res = httpd_resp_send_chunk(req, part_header, header_len);
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        // Send JPEG data
        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        esp_camera_fb_return(fb);

        if (res != ESP_OK) {
            break;
        }

        frame_count++;

        // Log frame rate every 5 seconds
        int64_t now = esp_timer_get_time();
        if (now - last_log_time >= 5000000) { // 5 seconds in microseconds
            float fps = (float)frame_count / ((now - last_log_time) / 1000000.0f);
            ESP_LOGI(HTTP_TAG, "MJPEG stream: %.1f fps, frame size: %zu bytes", fps, fb->len);
            frame_count = 0;
            last_log_time = now;
        }

        // Small yield to allow other tasks to run
        vTaskDelay(1);
    }

    ESP_LOGI(HTTP_TAG, "MJPEG stream ended");
    return res;
}

// Single JPEG capture handler - for testing/debugging
static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(HTTP_TAG, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);

    return res;
}

// Status endpoint - returns JSON with camera info
static esp_err_t status_handler(httpd_req_t *req) {
    sensor_t *sensor = esp_camera_sensor_get();

    char json[256];
    snprintf(json, sizeof(json),
        "{\"framesize\":%d,\"quality\":%d,\"brightness\":%d,\"contrast\":%d,"
        "\"saturation\":%d,\"sharpness\":%d,\"vflip\":%d,\"hmirror\":%d}",
        sensor->status.framesize,
        sensor->status.quality,
        sensor->status.brightness,
        sensor->status.contrast,
        sensor->status.saturation,
        sensor->status.sharpness,
        sensor->status.vflip,
        sensor->status.hmirror
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, strlen(json));
}

static httpd_handle_t stream_httpd = NULL;

static void start_http_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.max_open_sockets = 4;
    config.max_uri_handlers = 8;
    config.stack_size = 8192;
    config.core_id = 0;
    config.lru_purge_enable = true;

    ESP_LOGI(HTTP_TAG, "Starting HTTP server on port %d", config.server_port);

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(stream_httpd, &index_uri);

        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = stream_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(stream_httpd, &stream_uri);

        httpd_uri_t capture_uri = {
            .uri = "/capture",
            .method = HTTP_GET,
            .handler = capture_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(stream_httpd, &capture_uri);

        httpd_uri_t status_uri = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(stream_httpd, &status_uri);

        ESP_LOGI(HTTP_TAG, "HTTP server started successfully");
        ESP_LOGI(HTTP_TAG, "  Web UI:  http://<ip>/");
        ESP_LOGI(HTTP_TAG, "  Stream:  http://<ip>/stream");
        ESP_LOGI(HTTP_TAG, "  Capture: http://<ip>/capture");
        ESP_LOGI(HTTP_TAG, "  Status:  http://<ip>/status");
    } else {
        ESP_LOGE(HTTP_TAG, "Failed to start HTTP server");
    }
}
