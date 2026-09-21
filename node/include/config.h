#pragma once

// Change only this value when flashing the second node.
#define NODE_ID 1

#define GATEWAY_ID 0

// India ISM-band prototype channel. Keep this identical on all devices.
#define LORA_FREQUENCY_MHZ 866.5
#define LORA_BANDWIDTH_KHZ 125.0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODING_RATE 5
#define LORA_SYNC_WORD 0x12
#define LORA_OUTPUT_POWER_DBM 10
#define LORA_PREAMBLE_LENGTH 8

#define SUPERFRAME_MS 5000UL
#define NODE1_SLOT_OFFSET_MS 500UL
#define NODE2_SLOT_OFFSET_MS 2500UL
#define SYNC_LOSS_TIMEOUT_MS 15000UL

// Standard ESP32 VSPI pins.
#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_NSS 5
#define LORA_DIO1 27
#define LORA_RST 14
#define LORA_BUSY 26

// Shared I2C bus for MPU6050 and BMP280.
#define I2C_SDA 21
#define I2C_SCL 22

#define FLAME_PIN 34
#define VIBRATION_PIN 32
#define GAS_ANALOG_PIN 33

#define STATUS_LED_PIN 2 //onboard led of esp32dev
#define USER_BUTTON_PIN 13

// Many flame modules assert LOW. Change if your module is active HIGH.
#define FLAME_ACTIVE_LOW true

// Many vibration modules assert HIGH. Change if your module is active LOW.
#define VIBRATION_ACTIVE_HIGH true