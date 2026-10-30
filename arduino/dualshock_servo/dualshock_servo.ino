// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2
//
// Arduino IDE port of main/dualshock_servo.cpp: steers a servo with the left
// stick of a PlayStation controller. Needs the "esp32_bluepad32" board package,
// see the README.
//
// Differences from the ESP-IDF version:
// - Prints with Serial. The board package has no Bluepad32 console.
// - The board package (4.1.0) is built on Arduino ESP32 core 2.0.17, which has
//   a channel-based LEDC API. Core 3.x attaches LEDC by pin instead.

#include <Bluepad32.h>
#include <Ticker.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// Onboard LED. GPIO 2 on most ESP32 DevKit V1 boards.
static const int LED_PIN = 2;
Ticker ledTicker;
bool ledOn = false;

int connectedControllers() {
    int count = 0;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] != nullptr)
            count++;
    }
    return count;
}

// Runs every 100 ms from a hardware timer, independent of loop() timing.
// Fast blink while scanning, solid on while a controller is connected.
void updateLed() {
    ledOn = connectedControllers() > 0 ? true : !ledOn;
    digitalWrite(LED_PIN, ledOn);
}

// Hobby servo signal on GPIO 4: a 50 Hz pulse, 544-2400 us for 0-180 degrees
// (the same range as the Arduino Servo library).
static const int SERVO_PIN = 4;
static const int SERVO_LEDC_CHANNEL = 0;  // Only used by core 2.x.
static const int SERVO_FREQ_HZ = 50;
static const int SERVO_RESOLUTION_BITS = 16;
static const int SERVO_MIN_US = 544;
static const int SERVO_MAX_US = 2400;
int servoAngle = -1;

bool servoAttach() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    return ledcAttach(SERVO_PIN, SERVO_FREQ_HZ, SERVO_RESOLUTION_BITS);
#else
    // ledcSetup() returns the frequency it set, or 0 on failure.
    if (ledcSetup(SERVO_LEDC_CHANNEL, SERVO_FREQ_HZ, SERVO_RESOLUTION_BITS) == 0)
        return false;
    ledcAttachPin(SERVO_PIN, SERVO_LEDC_CHANNEL);
    return true;
#endif
}

void servoWrite(int angle) {
    if (angle == servoAngle)
        return;
    servoAngle = angle;
    uint32_t pulseUs = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
    // Duty is the pulse width as a fraction of the 20 ms period.
    uint32_t duty = pulseUs * ((1 << SERVO_RESOLUTION_BITS) - 1) / (1000000 / SERVO_FREQ_HZ);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(SERVO_PIN, duty);
#else
    ledcWrite(SERVO_LEDC_CHANNEL, duty);
#endif
    Serial.printf("Servo: %d degrees, pulse %lu us\n", angle, (unsigned long)pulseUs);
}

// This callback gets called any time a new controller is connected.
// Up to 4 controllers can be connected at the same time.
void onConnectedController(ControllerPtr ctl) {
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Serial.printf("CALLBACK: Controller is connected, index=%d\n", i);
            ControllerProperties properties = ctl->getProperties();
            Serial.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName().c_str(),
                          properties.vendor_id, properties.product_id);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
    if (!foundEmptySlot) {
        Serial.println("CALLBACK: Controller connected, but could not found empty slot");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    bool foundController = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Serial.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            break;
        }
    }

    if (!foundController) {
        Serial.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }

    if (connectedControllers() == 0) {
        Serial.println("No controllers connected, scanning again");
    }
}

// Prints the pairing state. Called every 2 seconds from loop().
void printStatus() {
    unsigned long uptime = millis() / 1000;
    if (connectedControllers() == 0) {
        Serial.printf("[%lus] Scanning... press PS on a paired DualShock 4, or hold Share + PS to pair a new one\n", uptime);
        return;
    }
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        ControllerPtr ctl = myControllers[i];
        if (ctl == nullptr)
            continue;
        // battery(): 0 = unknown, 1 = empty, 255 = full.
        uint8_t battery = ctl->battery();
        if (battery == 0) {
            Serial.printf("[%lus] Controller %d connected: %s, battery unknown\n", uptime, i,
                          ctl->getModelName().c_str());
        } else {
            Serial.printf("[%lus] Controller %d connected: %s, battery %d%%\n", uptime, i,
                          ctl->getModelName().c_str(), (battery - 1) * 100 / 254);
        }
    }
}

// Left stick X steers the servo: full left = 0 degrees, center = 90, full right = 180.
void processGamepad(ControllerPtr ctl) {
    int x = ctl->axisX();
    if (abs(x) < 32)  // Ignore stick noise around the center.
        x = 0;
    servoWrite(map(x, -512, 512, 0, 180));
}

void processControllers() {
    for (auto myController : myControllers) {
        if (myController && myController->isConnected() && myController->hasData() && myController->isGamepad()) {
            processGamepad(myController);
        }
    }
}

// Arduino setup function. Runs in CPU 1
void setup() {
    Serial.begin(115200);
    Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    // Bluepad32 starts scanning and connects to any supported controller it finds.
    BP32.setup(&onConnectedController, &onDisconnectedController);

    // "BP32.forgetBluetoothKeys()" is not called, so paired controllers
    // reconnect after a reset. Calling it once might fix connection issues.

    // Keeps DualShock 4 and DualSense touchpads from showing up as a second, mouse device.
    BP32.enableVirtualDevice(false);

    // Onboard LED shows the pairing state, see updateLed().
    pinMode(LED_PIN, OUTPUT);
    ledTicker.attach_ms(100, updateLed);

    // Servo self-test: 0, 180, then center, so the wiring can be checked
    // without a controller.
    if (!servoAttach()) {
        Serial.printf("Servo: PWM setup on GPIO %d failed\n", SERVO_PIN);
    }
    for (int angle : {0, 180, 90}) {
        servoWrite(angle);
        delay(600);
    }
}

// Arduino loop function. Runs in CPU 1.
void loop() {
    // This call fetches all the controllers' data.
    bool dataUpdated = BP32.update();
    if (dataUpdated)
        processControllers();

    static unsigned long lastStatus = 0;
    if (millis() - lastStatus >= 2000) {
        lastStatus = millis();
        printStatus();
    }

    // The delay also yields to lower priority tasks, which keeps the task watchdog from triggering.
    delay(20);  // One servo period at 50 Hz.
}
