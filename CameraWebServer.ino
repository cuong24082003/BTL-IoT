#include "esp_camera.h"
#include <WiFi.h>
#include <base64.h>
#include <PubSubClient.h>  // Thêm thư viện MQTT

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// WiFi credentials
const char* ssid = "IoT";
const char* password = "234567Cn";

// MQTT broker details
const char* mqtt_server = "192.168.0.102";
const int mqtt_port = 1883;
const char* mqtt_topic = "Cuong_CC"; // Chủ đề để gửi ảnh

WiFiClient espClient;
PubSubClient client(espClient);

void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect("ESP32Client")) {
      Serial.println("Connected to MQTT Broker!");
    } else {
      Serial.print("Failed to connect to MQTT. Retrying...");
      delay(5000);
    }
  }
}

void setupCamera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    return;
  }
  Serial.println("Camera ready!");
}

void sendEncodedImage() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // Encode image to Base64
  String encodedImage = base64::encode(fb->buf, fb->len);

  // Chia nhỏ Base64 thành các phần nhỏ và gửi qua MQTT
  int partSize = 200; // Kích thước mỗi phần (tùy chỉnh)
  int totalParts = (encodedImage.length() + partSize - 1) / partSize;

  for (int i = 0; i < totalParts; i++) {
    String part = encodedImage.substring(i * partSize, (i + 1) * partSize);
    String message = String(i + 1) + "/" + String(totalParts) + ":" + part;
    client.publish(mqtt_topic, message.c_str());
    delay(100); // Thời gian giữa các phần gửi
  }

  // Gửi thông báo kết thúc
  client.publish(mqtt_topic, "end");

  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);

  connectToWiFi(); // Kết nối Wi-Fi
  client.setServer(mqtt_server, mqtt_port); // Cấu hình MQTT
  reconnectMQTT(); // Kết nối với broker MQTT

  setupCamera(); // Khởi tạo camera
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT(); // Kiểm tra và kết nối lại MQTT nếu mất kết nối
  }
  client.loop(); // Xử lý các tin nhắn MQTT

  // Capture and encode image periodically
  sendEncodedImage();
  delay(5000); // Gửi ảnh mỗi 10 giây
}
