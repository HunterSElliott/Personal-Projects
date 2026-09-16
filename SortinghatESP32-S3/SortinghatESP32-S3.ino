#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "sortinghat_skeleton1.0.9.h"  // CHANGE THIS to your exact .h filename!

// Initialize PCA9685
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// PCA9685 Channel Mapping
#define MOUTH_PORT 0
#define EYE_LEFT_PORT 1
#define EYE_RIGHT_PORT 2
#define HEAD_LOWER_PORT 3
#define HEAD_UPPER_PORT 4

// PCA9685 Pulse Limits (Adjust based on your specific servos)
#define JAW_CLOSED_PULSE 300
#define JAW_OPEN_PULSE 450

// ---------------- CONFIGURATION ----------------
// const char* ssid = "3207_legacy";
// const char* password = "Veu7sxNs";
// const char* server_url = "http://10.34.187.231:8000/chat";
const char* ssid = "SortingHat_Net";
const char* password = "Lumos1234";
const char* server_url = "http://10.42.0.1:8000/chat";


// I2S Microphone (INMP441) Pins
#define MIC_BCLK 4
#define MIC_LRC 5
#define MIC_DOUT 6

// I2S Speaker (MAX98357A) Pins
#define SPK_BCLK 15
#define SPK_LRC 16
#define SPK_DIN 17

// Trigger Pin
#define BUTTON_PIN 12

// Audio Recording Settings
#define SAMPLE_RATE 16000
#define RECORD_TIME 4  // Record for 4 seconds
const int headerSize = 44;
const int recordSize = (SAMPLE_RATE * 2 * RECORD_TIME);
uint8_t* audioBuffer = nullptr;

void setup() {
  Serial.begin(115200);
  Serial.println("Began setup");
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize I2C and PCA9685 (SDA=8, SCL=9)
  Wire.begin(8, 9);
  pwm.begin();
  pwm.setPWMFreq(50);
  pwm.setPWM(MOUTH_PORT, 0, JAW_CLOSED_PULSE);
  // Send the 4 body servos to a safe "center" pulse (roughly 375) to prevent startup buzzing
  pwm.setPWM(EYE_LEFT_PORT, 0, 375);
  pwm.setPWM(EYE_RIGHT_PORT, 0, 375);
  pwm.setPWM(HEAD_LOWER_PORT, 0, 375);
  pwm.setPWM(HEAD_UPPER_PORT, 0, 375);

  Serial.println("Begin WIFI");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  audioBuffer = (uint8_t*)malloc(recordSize + headerSize);
  if (audioBuffer == nullptr) {
    Serial.println("Failed to allocate memory!");
    while (1)
      ;
  }

  generateWavHeader(audioBuffer, recordSize, SAMPLE_RATE);
  Serial.println("initI2S");
  initI2S();
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Recording...");
    recordAudio();
    Serial.println("Recording Finished. Sending to Brain...");

    sendAudioToPiAndPlayResponse();

    while (digitalRead(BUTTON_PIN) == LOW) { delay(10); }
    delay(1000);
  }
}

// ---------------- AUDIO & ANIMATION FUNCTIONS ----------------

void initI2S() {
  Serial.println("Entered initI2S");
  // Speaker
  i2s_config_t spk_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 22050,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false
  };
  i2s_pin_config_t spk_pins = {
    .bck_io_num = SPK_BCLK, .ws_io_num = SPK_LRC, .data_out_num = SPK_DIN, .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_0, &spk_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &spk_pins);

  // Microphone
  i2s_config_t mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false
  };
  i2s_pin_config_t mic_pins = {
    .bck_io_num = MIC_BCLK, .ws_io_num = MIC_LRC, .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = MIC_DOUT
  };
  i2s_driver_install(I2S_NUM_1, &mic_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &mic_pins);
  Serial.println("Leaving initI2S");
}

void recordAudio() {
  size_t bytesRead = 0;
  i2s_read(I2S_NUM_1, &audioBuffer[headerSize], recordSize, &bytesRead, portMAX_DELAY);
}

void sendAudioToPiAndPlayResponse() {
  WiFiClient client;
  // const char* host = "10.34.187.231";
  const char* host = "10.42.0.1";
  const int port = 8000;

  Serial.println("Connecting to Pi...");
  if (!client.connect(host, port)) {
    Serial.println("Connection to Pi failed!");
    return;
  }

  String boundary = "----ESP32Boundary";
  String head = "--" + boundary + "\r\n"
                                  "Content-Disposition: form-data; name=\"audio_file\"; filename=\"request.wav\"\r\n"
                                  "Content-Type: audio/wav\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  int totalLen = head.length() + (recordSize + headerSize) + tail.length();

  client.print("POST /chat HTTP/1.1\r\n");
  client.print("Host: " + String(host) + "\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(totalLen) + "\r\n\r\n");

  client.print(head);
  client.write(audioBuffer, recordSize + headerSize);
  client.print(tail);

  Serial.println("Audio sent! Waiting for Hat to think...");
  while (client.connected() && !client.available()) {
    delay(10);
  }

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  size_t bytesRead = 0;
  uint8_t outBuffer[1024];
  client.readBytes(outBuffer, 44);  // Skip WAV header

  Serial.println("Playing response and running animation...");

  // --- ANIMATION VARIABLES ---
  unsigned long lastFrameTime = 0;
  // FPS and LENGTH are pulled directly from your .h file
  int frameDelay = 1000 / FPS;
  int animIndex = 0;

  while (client.available()) {
    // 1. Audio and Lip Sync
    bytesRead = client.readBytes(outBuffer, sizeof(outBuffer));
    if (bytesRead > 0) {
      size_t bytesWritten;
      i2s_write(I2S_NUM_0, outBuffer, bytesRead, &bytesWritten, portMAX_DELAY);
      pwm.setPWM(MOUTH_PORT, 0, calculateJawVolume(outBuffer, bytesRead));
    }

    // 2. Body Animation (Binary Parser)
    if (millis() - lastFrameTime >= frameDelay) {
      lastFrameTime = millis();

      while (animIndex < LENGTH) {
        byte b = pgm_read_byte(&ANIMATION_DATA[animIndex++]);

        if (b == 0x3C) {  // '<' start
          byte servoId = pgm_read_byte(&ANIMATION_DATA[animIndex++]);
          byte posHigh = pgm_read_byte(&ANIMATION_DATA[animIndex++]);
          byte posLow = pgm_read_byte(&ANIMATION_DATA[animIndex++]);
          byte endMark = pgm_read_byte(&ANIMATION_DATA[animIndex++]);

          if (endMark == 0x3E) {  // '>' end
            int angle = (posHigh << 8) | posLow;
            // Convert Blender degrees to PCA pulse widths
            int pulse = map(angle, 0, 180, 150, 600);

            // Map Blender Bone IDs to PCA9685 Ports
            if (servoId == 1) pwm.setPWM(EYE_LEFT_PORT, 0, pulse);
            else if (servoId == 2) pwm.setPWM(EYE_RIGHT_PORT, 0, pulse);
            else if (servoId == 3) pwm.setPWM(HEAD_LOWER_PORT, 0, pulse);
            else if (servoId == 4) pwm.setPWM(HEAD_UPPER_PORT, 0, pulse);
          }
        } else if (b == 0x0A) {  // '\n' end of frame
          break;
        }
      }

      if (animIndex >= LENGTH) { animIndex = 0; }
    }
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  pwm.setPWM(MOUTH_PORT, 0, JAW_CLOSED_PULSE);

  client.stop();
  Serial.println("Done!");
}

int calculateJawVolume(uint8_t* buffer, size_t length) {
  long sum = 0;
  for (size_t i = 0; i < length; i += 2) {
    int16_t sample = (buffer[i + 1] << 8) | buffer[i];
    sum += abs(sample);
  }
  int avgVolume = sum / (length / 2);
  int targetPulse = map(avgVolume, 0, 1000, JAW_CLOSED_PULSE, JAW_OPEN_PULSE);
  return constrain(targetPulse, JAW_CLOSED_PULSE, JAW_OPEN_PULSE);
}

void generateWavHeader(uint8_t* header, int wavSize, int sampleRate) {
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSize = wavSize + 36;
  header[4] = (uint8_t)(fileSize & 0xFF);
  header[5] = (uint8_t)((fileSize >> 8) & 0xFF);
  header[6] = (uint8_t)((fileSize >> 16) & 0xFF);
  header[7] = (uint8_t)((fileSize >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 16;
  header[17] = 0;
  header[18] = 0;
  header[19] = 0;
  header[20] = 1;
  header[21] = 0;
  header[22] = 1;
  header[23] = 0;
  header[24] = (uint8_t)(sampleRate & 0xFF);
  header[25] = (uint8_t)((sampleRate >> 8) & 0xFF);
  header[26] = (uint8_t)((sampleRate >> 16) & 0xFF);
  header[27] = (uint8_t)((sampleRate >> 24) & 0xFF);
  unsigned int byteRate = sampleRate * 2;
  header[28] = (uint8_t)(byteRate & 0xFF);
  header[29] = (uint8_t)((byteRate >> 8) & 0xFF);
  header[30] = (uint8_t)((byteRate >> 16) & 0xFF);
  header[31] = (uint8_t)((byteRate >> 24) & 0xFF);
  header[32] = 2;
  header[33] = 0;
  header[34] = 16;
  header[35] = 0;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (uint8_t)(wavSize & 0xFF);
  header[41] = (uint8_t)((wavSize >> 8) & 0xFF);
  header[42] = (uint8_t)((wavSize >> 16) & 0xFF);
  header[43] = (uint8_t)((wavSize >> 24) & 0xFF);
}