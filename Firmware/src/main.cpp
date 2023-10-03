#include "global.h"

void setup() {
    // delay(5000);
    set_sys_clock_khz(132000, true);
    Serial.begin(115200);
    Serial.println("Starting up");
    EEPROM.begin(4096);
    readEEPROM();

    gyroInit();
    osdInit();
    initBaro();

    initADC();

    // init ELRS on pins 8 and 9 using Serial2 (UART1)
    ELRS = new ExpressLRS(Serial2, 420000, 8, 9);

    // init LEDs
    gpio_init(PIN_LED_ACTIVITY);
    gpio_set_dir(PIN_LED_ACTIVITY, GPIO_OUT);
    gpio_init(PIN_LED_DEBUG);
    gpio_set_dir(PIN_LED_DEBUG, GPIO_OUT);
    gpio_put(PIN_LED_DEBUG, 1);

    initSpeaker();

    // init gyro/accel pointer
    gyroDataRaw  = bmiDataRaw + 3;
    accelDataRaw = bmiDataRaw;

    initESCs();
    initBlackbox();
    rp2040.wdt_begin(200);

    Serial.println("Setup complete");
    // startBootupSound(); //annoying during development
}

elapsedMillis activityTimer;
void          loop() {
    // baroLoop();
    ELRS->loop();
    if (Serial.available()) {
        delay(2);
        String s = Serial.readStringUntil('\n');
        if (s.charAt(0) == 'a' && s.charAt(1) == 'x') {
            accelSign[0] = -accelSign[0];
            Serial.printf("accelSign[0]:               %d\n", accelSign[0].getInt());
        } else if (s.charAt(0) == 'a' && s.charAt(1) == 'y') {
            accelSign[1] = -accelSign[1];
            Serial.printf("accelSign[1]:               %d\n", accelSign[1].getInt());
        } else if (s.charAt(0) == 'a' && s.charAt(1) == 'z') {
            accelSign[2] = -accelSign[2];
            Serial.printf("accelSign[2]:               %d\n", accelSign[2].getInt());
        } else if (s.charAt(0) == 'g' && s.charAt(1) == 'x') {
            gyroSign[0] = -gyroSign[0];
            Serial.printf("gyroSign[0]:               %d\n", gyroSign[0].getInt());
        } else if (s.charAt(0) == 'g' && s.charAt(1) == 'y') {
            gyroSign[1] = -gyroSign[1];
            Serial.printf("gyroSign[1]:               %d\n", gyroSign[1].getInt());
        } else if (s.charAt(0) == 'g' && s.charAt(1) == 'z') {
            gyroSign[2] = -gyroSign[2];
            Serial.printf("gyroSign[2]:               %d\n", gyroSign[2].getInt());
        } else if (s.charAt(0) == 'e') {
            float e          = s.substring(1).toFloat();
            ACCEL_UPDATE_EPS = fixedPointInt32::from(e);
            Serial.printf("ACCEL_UPDATE_EPS:               %.4f\n", ACCEL_UPDATE_EPS.getFloat());
        }
    }
    speakerLoop();
    gyroLoop();
    adcLoop();
    osdLoop();
    serialLoop();
    configuratorLoop();
    rp2040.wdt_reset();
    if (activityTimer > 500) {
        gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
        activityTimer = 0;
    }
}