#include "radar_controller.h"
#include "config.h"
#include "motor_controller.h"
#include "safety_controller.h"
#include "oled_manager.h"
#include <esp_arduino_version.h>

static int current_angle = 90;
static int sweep_direction = 1;  // 1 = sweeping right to left, -1 = sweeping left to right
static unsigned long last_servo_update = 0;

static float current_dist = 200.0f;
static float left_dist = 200.0f;
static float center_dist = 200.0f;
static float right_dist = 200.0f;

static RadarData current_radar_data = {
    90, 200.0f, 200.0f, 200.0f, 200.0f, "CLEAR", "CENTER"
};

static void setServoDuty(uint32_t duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(SERVO_PIN, duty);
#else
    ledcWrite(PWM_CHANNEL_SERVO, duty);
#endif
}

void updateServo(int angle) {
    int calibrated_angle = constrain(angle + SERVO_CENTER_OFFSET_DEG, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    current_angle = angle;

    // 50Hz LEDC Servo PWM Duty Calculation (16-bit resolution: 0 - 65535)
    // 0.5ms (500us) = 0 deg -> 1638 duty | 2.4ms (2400us) = 180 deg -> 7864 duty
    uint32_t duty = 1638 + ((uint32_t)calibrated_angle * (7864 - 1638) / 180);
    setServoDuty(duty);
}

void initRadar() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);  // Standard high-impedance input (avoid internal pull-down loading)
    digitalWrite(TRIG_PIN, LOW);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // ESP32 Arduino Core 3.x Servo PWM setup (50Hz, 16-bit)
    ledcAttach(SERVO_PIN, 50, 16);
#else
    // ESP32 Arduino Core 2.x Servo PWM setup
    ledcSetup(PWM_CHANNEL_SERVO, 50, 16);
    ledcAttachPin(SERVO_PIN, PWM_CHANNEL_SERVO);
#endif

    updateServo(90);
    delay(100);
}

float readDistance() {
    // 1. If Echo is still HIGH from a previous cycle, wait up to 3ms for it to settle LOW
    if (digitalRead(ECHO_PIN) == HIGH) {
        unsigned long wait_low_start = micros();
        while (digitalRead(ECHO_PIN) == HIGH && (micros() - wait_low_start < 3000)) {
            delayMicroseconds(10);
        }
    }

    // 2. Clear trigger pin to ensure clean rising edge
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(4);

    // 3. Send 10us trigger pulse to start ultrasonic ping
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // 4. Measure echo duration
    // 20000us timeout (~3.4m) gives plenty of time for 2m range without blocking FreeRTOS or Wi-Fi
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 20000);

    if (duration == 0) {
        return ULTRASONIC_MAX_RANGE_CM;  // No reflection -> Clear path
    }

    float raw_cm = (float)duration * 0.0343f / 2.0f;

    // Physical sensor range clamping:
    // If obstacle is right against the sensor (< 2cm), clamp to 2cm (VERY CLOSE, do NOT reset to 200cm!)
    if (raw_cm < 2.0f) {
        raw_cm = 2.0f;
    } else if (raw_cm > ULTRASONIC_MAX_RANGE_CM) {
        raw_cm = ULTRASONIC_MAX_RANGE_CM;
    }

    return raw_cm;
}

void detectObstacle() {
    bool is_front_angle = (current_angle >= 70 && current_angle <= 110);
    bool front_blocked = (center_dist < OBSTACLE_VERY_CLOSE_CM || (is_front_angle && current_dist < OBSTACLE_VERY_CLOSE_CM));
    bool front_warning = (center_dist < OBSTACLE_WARNING_CM || (is_front_angle && current_dist < OBSTACLE_WARNING_CM));

    if (front_blocked) {
        strncpy(current_radar_data.status, "VERY CLOSE", sizeof(current_radar_data.status));
    } else if (front_warning) {
        strncpy(current_radar_data.status, "OBSTACLE DETECTED", sizeof(current_radar_data.status));
    } else if (left_dist < OBSTACLE_WARNING_CM || right_dist < OBSTACLE_WARNING_CM || (!is_front_angle && current_dist < OBSTACLE_WARNING_CM)) {
        strncpy(current_radar_data.status, "SIDE OBSTACLE", sizeof(current_radar_data.status));
    } else {
        strncpy(current_radar_data.status, "CLEAR", sizeof(current_radar_data.status));
    }

    // Determine direction with more clear space, handling dead-ends safely
    if (front_warning || front_blocked) {
        // Front is obstructed:
        // When object is in Front & Left (left_dist < right_dist), turn RIGHT into clear space!
        // When object is in Front & Right (right_dist < left_dist), turn LEFT into clear space!
        if (right_dist > left_dist && right_dist > OBSTACLE_AVOID_DISTANCE_CM) {
            strncpy(current_radar_data.best_path, "RIGHT", sizeof(current_radar_data.best_path));
        } else if (left_dist > right_dist && left_dist > OBSTACLE_AVOID_DISTANCE_CM) {
            strncpy(current_radar_data.best_path, "LEFT", sizeof(current_radar_data.best_path));
        } else if (right_dist > OBSTACLE_AVOID_DISTANCE_CM) {
            strncpy(current_radar_data.best_path, "RIGHT", sizeof(current_radar_data.best_path));
        } else if (left_dist > OBSTACLE_AVOID_DISTANCE_CM) {
            strncpy(current_radar_data.best_path, "LEFT", sizeof(current_radar_data.best_path));
        } else {
            // Dead-end: all sectors blocked! Recommend backing out.
            strncpy(current_radar_data.best_path, "BACKWARD", sizeof(current_radar_data.best_path));
        }
    } else {
        // Front is clear: bias forward unless a wide open side path exists
        if (left_dist > right_dist + 20.0f && left_dist > 100.0f) {
            strncpy(current_radar_data.best_path, "LEFT", sizeof(current_radar_data.best_path));
        } else if (right_dist > left_dist + 20.0f && right_dist > 100.0f) {
            strncpy(current_radar_data.best_path, "RIGHT", sizeof(current_radar_data.best_path));
        } else {
            strncpy(current_radar_data.best_path, "CENTER", sizeof(current_radar_data.best_path));
        }
    }

    current_radar_data.angle = current_angle;
    current_radar_data.distance = current_dist;
    current_radar_data.left_dist = left_dist;
    current_radar_data.center_dist = center_dist;
    current_radar_data.right_dist = right_dist;
}

static bool is_sweeping = false;

void scanEnvironment() {
    unsigned long now = millis();
    if (now - last_servo_update < SERVO_STEP_INTERVAL_MS) {
        return;  // Non-blocking timing tick
    }
    last_servo_update = now;

    // 1. Measure distance at current physical servo angle
    int measured_angle = current_angle;
    current_dist = readDistance();

    // 2. Update sector distance memory strictly for the angle where distance was measured
#if !INVERT_SERVO_SWEEP
    // Standard mounting: CCW (> 110°) is LEFT, CW (< 70°) is RIGHT
    if (measured_angle >= 110) {
        left_dist = (0.6f * current_dist) + (0.4f * left_dist);
    } else if (measured_angle <= 70) {
        right_dist = (0.6f * current_dist) + (0.4f * right_dist);
    } else {
        center_dist = (0.6f * current_dist) + (0.4f * center_dist);
    }
#else
    // Inverted mounting: CW (< 70°) is LEFT, CCW (> 110°) is RIGHT
    if (measured_angle <= 70) {
        left_dist = (0.6f * current_dist) + (0.4f * left_dist);
    } else if (measured_angle >= 110) {
        right_dist = (0.6f * current_dist) + (0.4f * right_dist);
    } else {
        center_dist = (0.6f * current_dist) + (0.4f * center_dist);
    }
#endif

    // 3. Evaluate obstacle detection state with synchronized angle & distance
    detectObstacle();

#if RADAR_SERIAL_DEBUG
    static unsigned long last_radar_debug_print = 0;
    if (now - last_radar_debug_print >= 250) {
        last_radar_debug_print = now;
        Serial.printf("[RADAR] Angle: %3d deg | Dist: %5.1f cm | Front: %5.1f cm | Status: %-17s | Path: %s\n",
                      measured_angle, current_dist, center_dist, current_radar_data.status, current_radar_data.best_path);
    }
#endif

    // 4. Advance servo to next step
#if CONTINUOUS_RADAR_SWEEP
    current_angle += (sweep_direction * SERVO_SWEEP_STEP_DEG);
    if (current_angle >= SERVO_MAX_ANGLE) {
        current_angle = SERVO_MAX_ANGLE;
        sweep_direction = -1;
    } else if (current_angle <= SERVO_MIN_ANGLE) {
        current_angle = SERVO_MIN_ANGLE;
        sweep_direction = 1;
    }
    updateServo(current_angle);
#else
    // On-Demand Obstacle Search Mode:
    // Servo stays locked facing straight ahead (90°) until an obstacle is detected in front
    if (current_angle != 90) {
        current_angle = 90;
        updateServo(90);
    }
#endif
}

bool isFrontBlocked() {
#if ENABLE_AUTONOMOUS_AVOIDANCE
    float trigger_dist = OBSTACLE_AVOID_DISTANCE_CM;
#else
    float trigger_dist = OBSTACLE_VERY_CLOSE_CM;
#endif
    return (center_dist < trigger_dist || (current_angle >= 70 && current_angle <= 110 && current_dist < trigger_dist));
}

static unsigned long last_avoidance_time = 0;

unsigned long getLastAvoidanceTime() {
    return last_avoidance_time;
}

RadarData getRadarData() {
    return current_radar_data;
}

void performAvoidanceManeuver() {
    // 1. Immediately stop the car to prevent collision
    stopMotors();
    resetSafetyWatchdog();
#if ENABLE_OLED_DISPLAY
    showObstacleScanningAlert();
#endif
    Serial.println("\n[AUTO-AVOID] ========================================");
    Serial.println("[AUTO-AVOID] Step 1: Stopped car immediately!");
    delay(200);  // Allow physical momentum to dissipate

    // 2. Servo actively scans LEFT for an empty, object-free space
    Serial.printf("[AUTO-AVOID] Step 2: Servo turning LEFT (%d°) to search for empty space...\n", SERVO_LEFT_ANGLE);
    updateServo(SERVO_LEFT_ANGLE);
    delay(350);  // Give SG90 ample time to reach Left angle
    resetSafetyWatchdog();
    float scanned_left = readDistance();
    left_dist = scanned_left;
    Serial.printf("[AUTO-AVOID] --> Left Space Clearance: %.1f cm\n", scanned_left);

    // 3. Servo actively scans RIGHT for an empty, object-free space
    Serial.printf("[AUTO-AVOID] Step 3: Servo turning RIGHT (%d°) to search for empty space...\n", SERVO_RIGHT_ANGLE);
    updateServo(SERVO_RIGHT_ANGLE);
    delay(450);  // Give SG90 ample time to sweep across to Right angle
    resetSafetyWatchdog();
    float scanned_right = readDistance();
    right_dist = scanned_right;
    Serial.printf("[AUTO-AVOID] --> Right Space Clearance: %.1f cm\n", scanned_right);

    // 4. Return servo to CENTER (90°)
    Serial.printf("[AUTO-AVOID] Step 4: Re-centering servo to %d degrees...\n", SERVO_CENTER_ANGLE);
    updateServo(SERVO_CENTER_ANGLE);
    delay(300);  // Give SG90 ample time to re-center
    resetSafetyWatchdog();

    // Update radar telemetry with the fresh scanned values
    current_radar_data.left_dist = left_dist;
    current_radar_data.right_dist = right_dist;
    detectObstacle();

    // 5. Automatically turn the car in the direction of the empty space using full 4WD counter-rotation
    // If object is in front and left (scanned_right > scanned_left), turn RIGHT into empty space!
    // If object is in front and right (scanned_left > scanned_right), turn LEFT into empty space!
    resetSafetyWatchdog();
    if (scanned_right > scanned_left && scanned_right > OBSTACLE_AVOID_DISTANCE_CM) {
        Serial.printf("[AUTO-AVOID] Step 5: Obstacle on Left / Clear space on Right (%.1f cm)! Turning RIGHT...\n", scanned_right);
        turnRight(220);
#if ENABLE_OLED_DISPLAY
        blinkObjectTurn("RIGHT", 500); // Active visual blink during physical turn
#else
        delay(500);
#endif
        stopMotors();
    } else if (scanned_left > scanned_right && scanned_left > OBSTACLE_AVOID_DISTANCE_CM) {
        Serial.printf("[AUTO-AVOID] Step 5: Obstacle on Right / Clear space on Left (%.1f cm)! Turning LEFT...\n", scanned_left);
        turnLeft(220);
#if ENABLE_OLED_DISPLAY
        blinkObjectTurn("LEFT", 500);  // Active visual blink during physical turn
#else
        delay(500);
#endif
        stopMotors();
    } else if (scanned_right > OBSTACLE_AVOID_DISTANCE_CM) {
        Serial.printf("[AUTO-AVOID] Step 5: Turning RIGHT into clear space (%.1f cm)...\n", scanned_right);
        turnRight(220);
#if ENABLE_OLED_DISPLAY
        blinkObjectTurn("RIGHT", 500);
#else
        delay(500);
#endif
        stopMotors();
    } else if (scanned_left > OBSTACLE_AVOID_DISTANCE_CM) {
        Serial.printf("[AUTO-AVOID] Step 5: Turning LEFT into clear space (%.1f cm)...\n", scanned_left);
        turnLeft(220);
#if ENABLE_OLED_DISPLAY
        blinkObjectTurn("LEFT", 500);
#else
        delay(500);
#endif
        stopMotors();
    } else {
        // Both sides are blocked! Dead-end trap: reverse away, then spin around
        Serial.println("[AUTO-AVOID] Step 5: Dead-end! All directions blocked. Reversing away...");
        moveBackward(200);
#if ENABLE_OLED_DISPLAY
        blinkObjectTurn("BACK", 500);
#else
        delay(500);
#endif
        stopMotors();
        delay(150);
        resetSafetyWatchdog();
        turnRight(220);
#if ENABLE_OLED_DISPLAY
        blinkObjectTurn("RIGHT", 450);
#else
        delay(450);
#endif
        stopMotors();
    }

    resetSafetyWatchdog();

    // Refresh forward clearance measurement after turn
    delay(150);
    center_dist = readDistance();
    current_dist = center_dist;
    current_radar_data.center_dist = center_dist;
    current_radar_data.distance = center_dist;
    detectObstacle();

    // 6. Move forward into the newly cleared empty space
    if (center_dist > OBSTACLE_AVOID_DISTANCE_CM) {
        Serial.printf("[AUTO-AVOID] Step 6: Path ahead is now clear (%.1f cm)! Driving forward into empty space...\n", center_dist);
        resetSafetyWatchdog();
        moveForward(200);
        delay(450);  // Drive forward into the clear space
        stopMotors();
    }

    resetSafetyWatchdog();
    last_avoidance_time = millis();  // Set debounce timestamp upon maneuver completion
    Serial.println("[AUTO-AVOID] Step 7: Avoidance complete! Car safely repositioned in empty space.\n");
}
