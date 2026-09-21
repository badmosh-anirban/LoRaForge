#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BMP280.h>

#include "config.h"
#include "protocol.h"

Module radioModule(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
SX1262 radio(&radioModule);

Adafruit_MPU6050 mpu;
Adafruit_BMP280 bmp;

bool mpuAvailable = false;
bool bmpAvailable = false;
uint16_t sequenceNumber = 0;
uint32_t currentFrame = 0;
uint32_t lastSyncAt = 0;
uint32_t transmitAt = 0;
bool waitingForSlot = false;

uint32_t nodeSlotOffsetMs() {
    return NODE_ID == 1 ? NODE1_SLOT_OFFSET_MS : NODE2_SLOT_OFFSET_MS;
}

bool timeReached(uint32_t now, uint32_t target) {
    return static_cast<int32_t>(now - target) >= 0;
}

bool mytimefix(unsigned long int now, unsigned long int target){
    return ((now - target) >= 0);
}

// was created during debugging
// bool timeReached_fix2(uint32_t now, uint32_t target) {
//     int32_t difference = static_cast<int32_t>(now - target);
//     return difference >= 0;
// }

void blinkStatus(uint8_t count) {
    for (uint8_t index = 0; index < count; ++index) {
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(40);
        digitalWrite(STATUS_LED_PIN, LOW);
        delay(40);
    }
}

void initializeSensors() {
    Wire.begin(I2C_SDA, I2C_SCL);

    mpuAvailable = mpu.begin();
    // if (mpuAvailable) {
    //     mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    //     mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    //     mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    // }

    Serial.println("------------------------------");
    if (!mpuAvailable){
        Serial.println("Failed to find MPU6050 chip");
        while(1){
            blinkStatus(3);
            delay(1000);
        }
    }else{
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        Serial.println("Mpu initialized");
        delay(100);
    }

    bmpAvailable = bmp.begin(0x76);
    // if (!bmpAvailable) {
    //     bmpAvailable = bmp.begin(0x77);
    // }

    if (!bmpAvailable) {
        Serial.println("Could not find a valid BMP280 sensor, check wiring!");
        while(1){
            blinkStatus(3);
            delay(1000);
        }
    }else{
        Serial.println(F("BMP280 Init OK"));
    }

        Serial.println("------------------------------");
}

void initializeRadio() {
    //SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    int16_t state = radio.begin(
        LORA_FREQUENCY_MHZ,
        LORA_BANDWIDTH_KHZ,
        LORA_SPREADING_FACTOR,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_OUTPUT_POWER_DBM,
        LORA_PREAMBLE_LENGTH
    );

    // if (state != RADIOLIB_ERR_NONE) {
    //     Serial.printf("Radio initialization failed: %d\n", state);
    //     while (true) {
    //         blinkStatus(2);
    //         delay(1000);
    //     }
    // }

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("SX1262 connected successfully!");
    } else {
        Serial.print("SX1262 initialization failed, code: ");
        Serial.println(state);
        while (true) {
            blinkStatus(2);
            delay(1000);
        }
    }

    Serial.println("------------------------------");

    radio.setCRC(true);
    radio.startReceive();
}

void readSensors(SensorPayload &payload) {
    payload.temperatureX10 = INT16_MIN; //-32768
    payload.pressurePa = 0;
    payload.gasAdc = analogRead(GAS_ANALOG_PIN);
    payload.flame = digitalRead(FLAME_PIN) == (FLAME_ACTIVE_LOW ? LOW : HIGH);
    payload.vibration = digitalRead(VIBRATION_PIN) == (VIBRATION_ACTIVE_HIGH ? HIGH : LOW);
    payload.accelX = 0;
    payload.accelY = 0;
    payload.accelZ = 0;
    payload.uptimeSeconds = millis() / 1000UL;

    if (bmpAvailable) {
        payload.temperatureX10 = static_cast<int16_t>(bmp.readTemperature() * 10.0f);
        payload.pressurePa = static_cast<uint32_t>(bmp.readPressure());
    }

    if (mpuAvailable) {
        sensors_event_t acceleration;
        sensors_event_t gyro;
        sensors_event_t temperature;
        mpu.getEvent(&acceleration, &gyro, &temperature);
        payload.accelX = static_cast<int16_t>(acceleration.acceleration.x * 100.0f);
        payload.accelY = static_cast<int16_t>(acceleration.acceleration.y * 100.0f);
        payload.accelZ = static_cast<int16_t>(acceleration.acceleration.z * 100.0f);
    }
}

void sendDataPacket() {
    //Serial.println("inside data packet fxn");
    
    DataPacket packet{};
    packet.header.version = PROTOCOL_VERSION;
    packet.header.type = PKT_DATA;
    packet.header.nodeId = NODE_ID;
    packet.header.gatewayId = GATEWAY_ID;
    packet.header.sequence = sequenceNumber++;
    packet.header.frameNumber = currentFrame;
    packet.header.payloadLength = sizeof(packet.payload);
    readSensors(packet.payload);

    //Serial.println("trying to transmit.....");

    int16_t state = radio.transmit(
        reinterpret_cast<const uint8_t *>(&packet),
        sizeof(packet)
    );

    if (state == RADIOLIB_ERR_NONE) {
        Serial.printf("DATA sent: node=%u frame=%lu seq=%u\n",
                      NODE_ID,
                      static_cast<unsigned long>(currentFrame),
                      packet.header.sequence);
        Serial.println();
        blinkStatus(1);
    } else {
        Serial.printf("DATA transmission failed: %d\n", state);
    }

    //Serial.println("***************************");


    radio.startReceive();
}

void handleSyncPacket() {
    SyncPacket packet{};
    size_t packetLength = radio.getPacketLength();

    if (packetLength != sizeof(packet)) {
        radio.startReceive();
        return;
    }

    int16_t state = radio.readData(
        reinterpret_cast<uint8_t *>(&packet),
        sizeof(packet)
    );
    radio.startReceive();

    if (state != RADIOLIB_ERR_NONE ||
        packet.header.version != PROTOCOL_VERSION ||
        packet.header.type != PKT_SYNC ||
        packet.header.gatewayId != GATEWAY_ID) {
        return;
    }

    currentFrame = packet.header.frameNumber;
    lastSyncAt = millis();
    transmitAt = lastSyncAt + nodeSlotOffsetMs();
    waitingForSlot = true;

    // Serial.printf("SYNC received: frame=%lu, transmit in %lu ms\n",
    //               static_cast<unsigned long>(currentFrame),
    //               static_cast<unsigned long>(nodeSlotOffsetMs()));

    // was created during debugging
    Serial.printf("SYNC received: node=%u | frame=%lu | millis/now=%lu | offset for this node=%lu | transmitAt=%lu | waitingForSlot=%d\n",
                    NODE_ID,
                    static_cast<unsigned long>(currentFrame),
                    static_cast<unsigned long>(lastSyncAt),
                    static_cast<unsigned long>(nodeSlotOffsetMs()),
                    static_cast<unsigned long>(transmitAt),
                    waitingForSlot
    );

}

void pollRadio() {
    if (digitalRead(LORA_DIO1) != HIGH) {
        return;
    }

    size_t packetLength = radio.getPacketLength();
    if (packetLength == sizeof(SyncPacket)) {
        handleSyncPacket();
    } else {
        radio.startReceive();
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(FLAME_PIN, INPUT);
    pinMode(VIBRATION_PIN, INPUT);
    pinMode(GAS_ANALOG_PIN, INPUT);
    pinMode(USER_BUTTON_PIN, INPUT_PULLUP);

    Serial.printf("Starting mine sensor node %u\n", NODE_ID);

    Serial.println(NODE_ID == 1 ? NODE1_SLOT_OFFSET_MS : NODE2_SLOT_OFFSET_MS);

    initializeSensors();
    initializeRadio();
    Serial.println("Waiting for gateway SYNC...");
}

// was removed during debugging
// void loop() {
//     uint32_t now = millis();
//     pollRadio();

// //    if (waitingForSlot && timeReached(now, transmitAt)) {
//     if (waitingForSlot) {

//         Serial.println("inside waitingForSlot cdn: ");
//         // Serial.print("check time reached fxn output: ");
//         Serial.println(timeReached(now, transmitAt));
//         // Serial.print("check my time fix fxn output: ");
//         // Serial.println(mytimefix(now, transmitAt));

//         //if(timeReached(now, transmitAt)){
//         if(timeReached(now, transmitAt)){

//             Serial.println("inside timeReached_fix2 cdn: ");
            
//             waitingForSlot = false;

//             //Serial.println("***************************");
//             //Serial.println("inside void loop before sending data");

//             sendDataPacket();
//         }
//     }

//     if (lastSyncAt != 0 && now - lastSyncAt > SYNC_LOSS_TIMEOUT_MS) {
//         waitingForSlot = false;
//     }

//     delay(2);
// }


// this loop works
void loop() {
    pollRadio();
    uint32_t now = millis();
    

    if (waitingForSlot) {

        Serial.printf("WAITING | now=%lu | transmitAt=%lu | diff=%ld | reached=%d\n",
            static_cast<unsigned long>(now),
            static_cast<unsigned long>(transmitAt),
            static_cast<long>(static_cast<int32_t>(now - transmitAt)),
            timeReached(now, transmitAt)
        );

        if (timeReached(now, transmitAt)) {
            Serial.println(">>> SLOT REACHED <<<");
            waitingForSlot = false;
            sendDataPacket();
        }
    }

    // was removed during debugging
    // this somehow resets the waitingForSlot!!, so i commented it as of now
    // if (lastSyncAt != 0 && now - lastSyncAt > SYNC_LOSS_TIMEOUT_MS) {
    //     Serial.println(">>> SYNC LOST - cancelling slot <<<");
    //     waitingForSlot = false;
    // }

    delay(100);
}

// was created during debugging
// Notice that the scheduling check before pollRadio(), doesnt work
// void loop() {
//     uint32_t now = millis();

//     if (waitingForSlot) {

//         Serial.printf("WAIT | now=%lu | target=%lu | reached=%d\n",
//             (unsigned long)now,
//             (unsigned long)transmitAt,
//             timeReached(now, transmitAt)
//         );

//         if (timeReached(now, transmitAt)) {
//             Serial.println("========== TRANSMIT TIME REACHED ==========");
//             waitingForSlot = false;
//             sendDataPacket();
//         }
//     }

//     pollRadio();

//     if (lastSyncAt != 0 && now - lastSyncAt > SYNC_LOSS_TIMEOUT_MS) {
//         waitingForSlot = false;
//     }

//     delay(100);
// }