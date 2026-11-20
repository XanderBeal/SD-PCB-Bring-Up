#include <Arduino.h>
#include <Wire.h>
#include "pd_board.h"

// Schematic: U17 TCPP03-M20, I2C_ADD tied to GND.
static TwoWire pdBus(PC11, PA8);
static constexpr uint8_t kTcppAddress = 0x34;
static bool busReady = false;
static bool pdRunning = false;
static bool ledHigh = true;
static uint32_t ledChangedAt;
static uint32_t lastPdAttempt;

static bool writeControl(uint8_t value) {
    pdBus.beginTransmission(kTcppAddress);
    pdBus.write(0x00);
    pdBus.write(value);
    return pdBus.endTransmission() == 0;
}

extern "C" int pd_board_present(void) {
    pdBus.beginTransmission(kTcppAddress);
    pdBus.write(0x02); // TCPP flag register; VBUS_OK is bit 5.
    if (pdBus.endTransmission(false) != 0 ||
        pdBus.requestFrom(kTcppAddress, uint8_t(1)) != 1) return 0;
    return (pdBus.read() & 0x20) != 0;
}

extern "C" int pd_board_init(void) {
    if (!busReady) {
        pinMode(PD2, OUTPUT);
        digitalWrite(PD2, HIGH);
        pdBus.begin();
        pdBus.setClock(400000);
        busReady = true;
    }
    // Normal mode, consumer path closed; provider, VCONN and discharge off.
    return writeControl(0x10);
}

extern "C" int32_t BSP_USBPD_PWR_Init(uint32_t) {
    return pd_board_init() ? 0 : -1;
}
extern "C" int32_t BSP_USBPD_PWR_VBUSInit(uint32_t) {
    return writeControl(0x10) ? 0 : -1;
}
extern "C" int32_t BSP_USBPD_PWR_VBUSDeInit(uint32_t) {
    // Keep the self-powered sink path closed; return CC protection to low power.
    return writeControl(0x20) ? 0 : -1;
}
extern "C" int32_t BSP_USBPD_PWR_SetRole(uint32_t, uint32_t role) {
    return role == POWER_ROLE_SINK ? 0 : -1;
}
extern "C" int32_t BSP_USBPD_PWR_VCCSetState(uint32_t, uint32_t) {
    return 0;
}

void setup() {
    pinMode(PC7, OUTPUT);
    digitalWrite(PC7, HIGH);
    ledChangedAt = millis();
    lastPdAttempt = millis();
    if (pd_board_init()) pdRunning = pd_app_init();
}

void loop() {
    const uint32_t now = millis();
    if (now - ledChangedAt >= (ledHigh ? 1000u : 100u)) {
        ledHigh = !ledHigh;
        digitalWrite(PC7, ledHigh ? HIGH : LOW);
        ledChangedAt = now;
    }
    if (pdRunning) {
        pd_app_run();
    } else if (now - lastPdAttempt >= 1000u) {
        lastPdAttempt = now;
        if (pd_board_init()) pdRunning = pd_app_init();
    }
}
