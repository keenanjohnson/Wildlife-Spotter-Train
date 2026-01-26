#pragma once

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_camera.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#define MJPEG_BOUNDARY "frame"
#define MJPEG_CONTENT_TYPE "multipart/x-mixed-replace;boundary=" MJPEG_BOUNDARY
#define MJPEG_PART_HEADER "Content-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n"
#define MJPEG_BOUNDARY_HEADER "\r\n--" MJPEG_BOUNDARY "\r\n"

static const char *HTTP_TAG = "HTTP";

// Forward declaration of web UI handler
static esp_err_t index_handler(httpd_req_t *req);

// MJPEG stream handler - runs in stream server context
static esp_err_t stream_handler(httpd_req_t *req) {
    esp_err_t res = ESP_OK;
    char part_header[64];

    res = httpd_resp_set_type(req, MJPEG_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

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
        if (now - last_log_time >= 5000000) {
            float fps = (float)frame_count / ((now - last_log_time) / 1000000.0f);
            ESP_LOGI(HTTP_TAG, "MJPEG stream: %.1f fps, frame size: %zu bytes", fps, fb->len);
            frame_count = 0;
            last_log_time = now;
        }

        // Small yield
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(HTTP_TAG, "MJPEG stream ended");
    return res;
}

// Single JPEG capture handler
static esp_err_t capture_handler(httpd_req_t *req) {
    ESP_LOGI(HTTP_TAG, "Capture handler called!");

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(HTTP_TAG, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(HTTP_TAG, "Captured frame: %zu bytes", fb->len);

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);

    ESP_LOGI(HTTP_TAG, "Capture sent, result: %d", res);
    return res;
}

// Train control endpoint
static esp_err_t train_handler(httpd_req_t *req) {
    char action[32] = {0};
    char json[128];

    // Parse query string for action parameter
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len > 0) {
        char query[64];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            char param[32];
            if (httpd_query_key_value(query, "action", param, sizeof(param)) == ESP_OK) {
                strncpy(action, param, sizeof(action) - 1);
            }
        }
    }

    const char *result = "ok";
    int rc = 0;

    if (strcmp(action, "forward") == 0) {
        rc = train_send_command("F");
        result = rc == 0 ? "forward" : "error";
    } else if (strcmp(action, "backward") == 0) {
        rc = train_send_command("B");
        result = rc == 0 ? "backward" : "error";
    } else if (strcmp(action, "stop") == 0) {
        rc = train_send_command("S");
        result = rc == 0 ? "stopped" : "error";
    }

    snprintf(json, sizeof(json),
        "{\"action\":\"%s\",\"result\":\"%s\",\"state\":\"%s\"}",
        action[0] ? action : "status",
        result,
        train_state_str()
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, strlen(json));
}

// Status endpoint
static esp_err_t status_handler(httpd_req_t *req) {
    ESP_LOGI(HTTP_TAG, "Status handler called!");
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
static httpd_handle_t api_httpd = NULL;

static void start_http_server(void) {
    // ========== API Server (port 80) - for index, capture, status ==========
    httpd_config_t api_config = HTTPD_DEFAULT_CONFIG();
    api_config.server_port = 80;
    api_config.ctrl_port = 32768;
    api_config.max_open_sockets = 4;
    api_config.max_uri_handlers = 8;
    api_config.stack_size = 4096;
    api_config.core_id = 0;
    api_config.lru_purge_enable = true;

    ESP_LOGI(HTTP_TAG, "Starting API server on port %d", api_config.server_port);

    if (httpd_start(&api_httpd, &api_config) == ESP_OK) {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(api_httpd, &index_uri);

        httpd_uri_t capture_uri = {
            .uri = "/capture",
            .method = HTTP_GET,
            .handler = capture_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(api_httpd, &capture_uri);

        httpd_uri_t status_uri = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(api_httpd, &status_uri);

        httpd_uri_t train_uri = {
            .uri = "/train",
            .method = HTTP_GET,
            .handler = train_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(api_httpd, &train_uri);

        ESP_LOGI(HTTP_TAG, "API server started");
    } else {
        ESP_LOGE(HTTP_TAG, "Failed to start API server");
    }

    // ========== Stream Server (port 81) - dedicated to MJPEG streaming ==========
    httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
    stream_config.server_port = 81;
    stream_config.ctrl_port = 32769;
    stream_config.max_open_sockets = 2;
    stream_config.max_uri_handlers = 2;
    stream_config.stack_size = 8192;
    stream_config.core_id = 1;  // Run on different core
    stream_config.lru_purge_enable = true;

    ESP_LOGI(HTTP_TAG, "Starting stream server on port %d", stream_config.server_port);

    if (httpd_start(&stream_httpd, &stream_config) == ESP_OK) {
        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = stream_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(stream_httpd, &stream_uri);

        // Also allow accessing stream at root of port 81
        httpd_uri_t stream_root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = stream_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(stream_httpd, &stream_root_uri);

        ESP_LOGI(HTTP_TAG, "Stream server started");
    } else {
        ESP_LOGE(HTTP_TAG, "Failed to start stream server");
    }

    ESP_LOGI(HTTP_TAG, "HTTP servers started successfully");
    ESP_LOGI(HTTP_TAG, "  Web UI:  http://<ip>/");
    ESP_LOGI(HTTP_TAG, "  Capture: http://<ip>/capture");
    ESP_LOGI(HTTP_TAG, "  Status:  http://<ip>/status");
    ESP_LOGI(HTTP_TAG, "  Stream:  http://<ip>:81/stream");
}
