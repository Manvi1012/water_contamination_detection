#include "esp_camera.h"
#include <Arduino.h>
#include <math.h>

// =====================================================
// XIAO ESP32-S3 SENSE
// TURBIDITY + TDS + PHOTODIODE + LED + CAMERA
// =====================================================

// XIAO PIN -> GPIO
// D2 -> GPIO3
// D3 -> GPIO4
// D4 -> GPIO5
// D7 -> GPIO44

#define TURBIDITY_PIN     3
#define TDS_PIN           4
#define PHOTODIODE_PIN    5
#define LED_PIN           44


// =====================================================
// TURBIDITY CALIBRATION
// =====================================================

float voltageToNTU(float voltage)
{
  // Example calibration.
  // Replace after calibration with known standards.

  if (voltage >= 3.00)
  {
    return 0.0;
  }

  else if (voltage >= 2.50)
  {
    return 10.0 + (2.50 - voltage) * (10.0 / 0.50);
  }

  else if (voltage >= 2.00)
  {
    return 20.0 + (2.00 - voltage) * (30.0 / 0.50);
  }

  else if (voltage >= 1.50)
  {
    return 50.0 + (1.50 - voltage) * (50.0 / 0.50);
  }

  else if (voltage >= 1.00)
  {
    return 100.0 + (1.00 - voltage) * (100.0 / 0.50);
  }

  else
  {
    return 200.0;
  }
}


// =====================================================
// TDS CALCULATION
// =====================================================

float calculateTDS(float voltage)
{
  // Initial testing temperature
  float temperature = 25.0;

  float compensationCoefficient =
      1.0 + 0.02 * (temperature - 25.0);

  float compensatedVoltage =
      voltage / compensationCoefficient;

  float tdsValue =
      (133.42 * pow(compensatedVoltage, 3)
      - 255.86 * pow(compensatedVoltage, 2)
      + 857.39 * compensatedVoltage) * 0.5;

  if (tdsValue < 0)
  {
    tdsValue = 0;
  }

  return tdsValue;
}


// =====================================================
// XIAO ESP32-S3 SENSE CAMERA PINS
// OV2640
// =====================================================

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1

#define XCLK_GPIO_NUM     10

#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15

#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13


// =====================================================
// CAMERA INITIALIZATION
// LOW MEMORY + PSRAM
// =====================================================

bool initCamera()
{
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

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

  // JPEG format
  config.pixel_format = PIXFORMAT_JPEG;

  // Low memory setting
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;

  // Use PSRAM
  config.fb_location = CAMERA_FB_IN_PSRAM;

  // One frame buffer
  config.fb_count = 1;

  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;


  // Check PSRAM
  Serial.print("PSRAM: ");

  if (psramFound())
  {
    Serial.println("FOUND");
  }
  else
  {
    Serial.println("NOT FOUND");
    return false;
  }


  // Initialize camera
  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK)
  {
    Serial.print("Camera initialization FAILED: 0x");
    Serial.println(err, HEX);

    return false;
  }

  Serial.println("Camera initialized successfully.");

  return true;
}


// =====================================================
// CAMERA CAPTURE
// =====================================================

void capturePhoto()
{
  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb)
  {
    Serial.println("Camera capture FAILED!");
    return;
  }

  Serial.println("Camera capture OK");

  Serial.print("Image size : ");
  Serial.print(fb->len);
  Serial.println(" bytes");

  Serial.print("Width      : ");
  Serial.println(fb->width);

  Serial.print("Height     : ");
  Serial.println(fb->height);

  esp_camera_fb_return(fb);
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(3000);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" XIAO ESP32-S3 WATER QUALITY SYSTEM");
  Serial.println("========================================");

  // ADC
  analogReadResolution(12);

  // LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Camera
  Serial.println();
  Serial.println("Checking camera...");

  if (initCamera())
  {
    Serial.println("Camera status: OK");
  }
  else
  {
    Serial.println("Camera status: FAILED");
  }

  Serial.println();
  Serial.println("System ready.");
  Serial.println();
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
  const int samples = 20;


  // ===================================================
  // LED ON
  // ===================================================

  digitalWrite(LED_PIN, HIGH);

  delay(100);


  // ===================================================
  // TURBIDITY
  // ===================================================

  long turbidityTotal = 0;

  for (int i = 0; i < samples; i++)
  {
    turbidityTotal += analogRead(TURBIDITY_PIN);
    delay(5);
  }

  float turbidityADC =
      turbidityTotal / (float)samples;

  float turbidityVoltage =
      (turbidityADC / 4095.0) * 3.3;

  float NTU =
      voltageToNTU(turbidityVoltage);


  // ===================================================
  // TDS
  // ===================================================

  long tdsTotal = 0;

  for (int i = 0; i < samples; i++)
  {
    tdsTotal += analogRead(TDS_PIN);
    delay(5);
  }

  float tdsADC =
      tdsTotal / (float)samples;

  float tdsVoltage =
      (tdsADC / 4095.0) * 3.3;

  float TDS =
      calculateTDS(tdsVoltage);


  // ===================================================
  // PHOTODIODE + LM358
  // ===================================================

  long photoTotal = 0;

  for (int i = 0; i < samples; i++)
  {
    photoTotal += analogRead(PHOTODIODE_PIN);
    delay(5);
  }

  float photoADC =
      photoTotal / (float)samples;

  float photoVoltage =
      (photoADC / 4095.0) * 3.3;


  // ===================================================
  // SERIAL OUTPUT
  // ===================================================

  Serial.println();
  Serial.println("========================================");

  Serial.println("TURBIDITY");

  Serial.print("ADC      : ");
  Serial.println(turbidityADC, 0);

  Serial.print("Voltage  : ");
  Serial.print(turbidityVoltage, 3);
  Serial.println(" V");

  Serial.print("NTU      : ");
  Serial.print(NTU, 2);
  Serial.println(" NTU");


  Serial.println("----------------------------------------");

  Serial.println("TDS");

  Serial.print("ADC      : ");
  Serial.println(tdsADC, 0);

  Serial.print("Voltage  : ");
  Serial.print(tdsVoltage, 3);
  Serial.println(" V");

  Serial.print("TDS      : ");
  Serial.print(TDS, 2);
  Serial.println(" ppm");


  Serial.println("----------------------------------------");

  Serial.println("PHOTODIODE + LM358");

  Serial.print("ADC      : ");
  Serial.println(photoADC, 0);

  Serial.print("Voltage  : ");
  Serial.print(photoVoltage, 3);
  Serial.println(" V");


  Serial.println("----------------------------------------");

  Serial.println("LED      : ON");


  // ===================================================
  // CAMERA
  // ===================================================

  capturePhoto();


  Serial.println("========================================");


  // ===================================================
  // LED OFF
  // ===================================================

  digitalWrite(LED_PIN, LOW);

  delay(1000);
}