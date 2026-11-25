/*
 * =====================================================================================
 *  ESP32 HC-SR04 Ultrasonic Sensor & SG90 Servo - Direct Hardware Diagnostic Tool
 * =====================================================================================
 *  Use this sketch to verify if your HC-SR04 ultrasonic sensor and SG90 servo are
 *  properly wired, receiving power, and functioning correctly.
 *
 *  HARDWARE WIRING:
 *    HC-SR04 VCC   ---> ESP32 VIN or 5V (CRITICAL: Most HC-SR04 require 5V, not 3.3V!)
 *    HC-SR04 GND   ---> ESP32 GND
 *    HC-SR04 TRIG  ---> ESP32 GPIO 5
 *    HC-SR04 ECHO  ---> ESP32 GPIO 17
 *    SG90 SERVO SIG -> ESP32 GPIO 13 (PWM)
 *
 *  HOW TO USE:
 *    1. Open this file in Arduino IDE.
 *    2. Select Board: "ESP32 Dev Module" and your USB Port.
 *    3. Upload and open Tools -> Serial Monitor at 115200 baud.
 *    4. Put your hand 10 cm in front of the sensor. The distance should change live!
 * =====================================================================================
 */

#include <Arduino.h>
#include <esp_arduino_version.h>

#define TRIG_PIN 5
#define ECHO_PIN 17
#define SERVO_PIN 13
#define PWM_CHANNEL_SERVO 2

int current_angle = 90;
int sweep_dir = 1;

void updateServoAngle(int angle) {
    uint32_t duty = 1638 + ((uint32_t)angle * (7864 - 1638) / 180);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(SERVO_PIN, duty);
#else
    ledcWrite(PWM_CHANNEL_SERVO, duty);
#endif
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n============================================================");
    Serial.println("  ESP32 HC-SR04 ULTRASONIC & SG90 SERVO HARDWARE DIAGNOSTIC ");
    Serial.println("============================================================");
    Serial.println("[*] Pin Configuration:");
    Serial.println("    - TRIG PIN  : GPIO 5  (Output)");
    Serial.println("    - ECHO PIN  : GPIO 17 (Input with Pulldown)");
    Serial.println("    - SERVO PIN : GPIO 13 (50Hz PWM)");
    Serial.println("------------------------------------------------------------");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcAttach(SERVO_PIN, 50, 16);
#else
    ledcSetup(PWM_CHANNEL_SERVO, 50, 16);
    ledcAttachPin(SERVO_PIN, PWM_CHANNEL_SERVO);
#endif

    // Center servo to 90 degrees
    updateServoAngle(90);
    Serial.println("[+] SG90 Servo centered at 90 degrees.");
    Serial.println("[+] Starting continuous distance sampling (Place your hand in front)...\n");
}

void loop() {
    // 1. Gently sweep servo between 60 and 120 degrees to verify servo motion
    current_angle += (sweep_dir * 5);
    if (current_angle >= 120) {
        current_angle = 120;
        sweep_dir = -1;
    } else if (current_angle <= 60) {
        current_angle = 60;
        sweep_dir = 1;
    }
    updateServoAngle(current_angle);

    // 2. If Echo pin is still HIGH from a previous pulse, wait up to 3ms for it to drop
    if (digitalRead(ECHO_PIN) == HIGH) {
        unsigned long wait_start = micros();
        while (digitalRead(ECHO_PIN) == HIGH && (micros() - wait_start < 3000)) {
            delayMicroseconds(10);
        }
    }

    // 3. Clear trigger pin
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(4);

    // 4. Send 10 microsecond pulse to trigger ultrasonic burst
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // 5. Measure echo duration with interrupts temporarily disabled (timeout 25000us ~ 4 meters)
    noInterrupts();
    unsigned long echo_duration = pulseIn(ECHO_PIN, HIGH, 25000);
    interrupts();

    // 6. Evaluate result
    if (echo_duration == 0) {
        // Did not receive echo pulse
        int echo_state = digitalRead(ECHO_PIN);
        Serial.printf("[FAIL] Echo Timeout (0 us) | Angle: %3d° | ECHO Pin Level: %s\n",
                      current_angle, (echo_state == HIGH ? "STUCK HIGH (Check wiring / 5V)" : "LOW (No pulse received)"));
        Serial.println("       --> Check: Is HC-SR04 VCC connected to 5V? Are TRIG (GPIO 5) and ECHO (GPIO 17) swapped?");
    } else {
        float distance_cm = (float)echo_duration * 0.0343f / 2.0f;
        if (distance_cm < 2.0f) distance_cm = 2.0f;  // Physical minimum clamp
        
        // Build visual bar
        int bars = constrain((int)(distance_cm / 5.0f), 1, 20);
        char bar_str[22];
        for (int i = 0; i < bars; i++) bar_str[i] = '=';
        bar_str[bars] = '\0';

        const char* status = "CLEAR";
        if (distance_cm <= 10.0f) status = "🚨 VERY CLOSE (STOP)";
        else if (distance_cm <= 25.0f) status = "⚠️  OBSTACLE WARNING";

        Serial.printf("[OK] Angle: %3d° | Dist: %5.1f cm | Echo: %5lu us | [%-20s] %s\n",
                      current_angle, distance_cm, echo_duration, bar_str, status);
    }

    delay(80);
}
