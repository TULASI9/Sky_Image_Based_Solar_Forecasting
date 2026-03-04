#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "board_config.h"

// WiFi credentials
const char* ssid = "JioFiber-NuN5N";
const char* password = "adi123adi";

// Server endpoint
const char* serverUrl = "http://172.26.52.178:5000/upload";

// Variables
String pvValue = "";   // received power value from NodeMCU
unsigned long lastUpload = 0;
const unsigned long uploadInterval = 10000;

void setup() {
  Serial.begin(115200); // Default UART0 (TX=GPIO1, RX=GPIO3)

#if defined(LED_GPIO_NUM)
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);
#endif

  // Camera config (pins unchanged from board_config.h)
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 20;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed!");
    return;
  }

  // ==== WiFi Connect ====
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
    if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();

    if (msg.startsWith("pv=")) {
      pvValue = msg.substring(3);  // extract number after "pv="
      Serial.print("Received power value from NodeMCU: ");
      Serial.println(pvValue);
    }
  }

  if (millis() - lastUpload >= uploadInterval) {
    upload();                 // do upload
    lastUpload = millis();    // reset timer
  } // wait 10s before next upload
}

void upload() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return;
  }

  // 🔹 Capture image
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);

  String boundary = "----ESP32FormBoundary";
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"image\"; filename=\"esp32.jpg\"\r\n"
                "Content-Type: image/jpeg\r\n\r\n";

  String middle = "\r\n--" + boundary + "\r\n"
                  "Content-Disposition: form-data; name=\"pv\"\r\n\r\n";

  String pvStr = pvValue;
  String tail = "\r\n--" + boundary + "--\r\n";

  // Calculate total length
  int contentLength = head.length() + fb->len + middle.length() + pvStr.length() + tail.length();

  // Allocate buffer
  uint8_t *payload = (uint8_t *)malloc(contentLength);
  if (!payload) {
    Serial.println("Not enough memory for payload buffer");
    esp_camera_fb_return(fb);
    return;
  }

  // Build payload in memory
  int index = 0;
  memcpy(payload + index, head.c_str(), head.length());
  index += head.length();

  memcpy(payload + index, fb->buf, fb->len);
  index += fb->len;

  memcpy(payload + index, middle.c_str(), middle.length());
  index += middle.length();

  memcpy(payload + index, pvStr.c_str(), pvStr.length());
  index += pvStr.length();

  memcpy(payload + index, tail.c_str(), tail.length());
  index += tail.length();

  // Add headers
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Connection", "close");
  http.addHeader("Content-Length", String(contentLength));

  // ---- Send request ----
  int httpResponseCode = http.POST(payload, contentLength);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Server response:");
    Serial.println(response);
  } else {
    Serial.printf("Error code: %d\n", httpResponseCode);
  }

  http.end();
  esp_camera_fb_return(fb);
  free(payload); // Free memory
}
