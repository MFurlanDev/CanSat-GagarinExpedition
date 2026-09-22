#include "esp_camera.h"
#include "SD_MMC.h"

// Select camera model
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

const char *photoPrefix = "/photo_";
int photoNumber = 0;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_UXGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_UXGA);

  Serial.println("Initialising SD card");
  if (!SD_MMC.begin()) {
    Serial.println("Failed to initialise SD card!");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("SD card slot appears to be empty!");
    return;
  }

  Serial.println("Sistema listo.");
  Serial.println("Esperando 3 minutos antes de empezar a tomar fotos...");

  delay(180000); //3 minutos de espera

  Serial.println("Comenzando fotos cada 1 segundo...");
}

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    return;
  }

  String photoFileName = String(photoPrefix) + String(photoNumber) + ".jpg";
  fs::FS &fs = SD_MMC;

  Serial.printf("Picture file name: %s\n", photoFileName.c_str());

  File file = fs.open(photoFileName.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  } else {
    file.write(fb->buf, fb->len);
    file.close();
    Serial.printf("Saved file to path: %s\n", photoFileName.c_str());
    photoNumber++;
  }

  esp_camera_fb_return(fb);

  delay(1000);  // 1 segundo entre fotos
}