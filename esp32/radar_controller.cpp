#include "radar_controller.h"
#include "config.h"
#include "motor_controller.h"
#include "safety_controller.h"
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

    // 4. Measure echo duration with FreeRTOS interrupt shield
    // 20000us timeout (~3.4m) gives plenty of time for 2m range without blocking Wi-Fi
    noInterrupts();
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 20000);
    interrupts();

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
    if (center_dist < OBSTACLE_WARNING_CM) {
        // Front is obstructed: pick clearest side, or BACKWARD if both sides are also blocked
        if (left_dist > right_dist + 10.0f && left_dist > OBSTACLE_WARNING_CM) {
            strncpy(current_radar_data.best_path, "LEFT", sizeof(current_radar_data.best_path));
        } else if (right_dist > left_dist + 10.0f && right_dist > OBSTACLE_WARNING_CM) {
            strncpy(current_radar_data.best_path, "RIGHT", sizeof(current_radar_data.best_path));
        } else if (left_dist > OBSTACLE_WARNING_CM) {
            strncpy(current_radar_data.best_path, "LEFT", sizeof(current_radar_data.best_path));
        } else if (right_dist > OBSTACLE_WARNING_CM) {
            strncpy(current_radar_data.best_path, "RIGHT", sizeof(current_radar_data.best_path));
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
    if (measured_angle <= 70) {
        left_dist = (0.6f * current_dist) + (0.4f * left_dist);
    } else if (measured_angle >= 110) {
        right_dist = (0.6f * current_dist) + (0.4f * right_dist);
    } else {
        center_dist = (0.6f * current_dist) + (0.4f * center_dist);
    }

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

RadarData getRadarData() {
    return current_radar_data;
}

void performAvoidanceManeuver() {
    // 1. Immediately stop the car to prevent collision
    stopMotors();
    Serial.println("[AUTO-AVOID] Obstacle detected in front! Step 1: Stopped immediately.");
    delay(150);  // Allow physical momentum to dissipate

    // 2. Servo actively scans LEFT for an empty, object-free space
    Serial.println("[AUTO-AVOID] Step 2: Servo scanning LEFT for empty space...");
    updateServo(35);  // Turn SG90 servo Left (35°)
    delay(220);       // Wait for servo to reach 35°
    float scanned_left = readDistance();
    left_dist = scanned_left;
    Serial.printf("[AUTO-AVOID] Left space clearance: %.1f cm\n", scanned_left);

    // 3. Servo actively scans RIGHT for an empty, object-free space
    Serial.println("[AUTO-AVOID] Step 3: Servo scanning RIGHT for empty space...");
    updateServo(145); // Turn SG90 servo Right (145°)
    delay(280);       // Wait for servo to sweep from 35° to 145°
    float scanned_right = readDistance();
    right_dist = scanned_right;
    Serial.printf("[AUTO-AVOID] Right space clearance: %.1f cm\n", scanned_right);

    // 4. Return servo to CENTER (90°)
    Serial.println("[AUTO-AVOID] Step 4: Re-centering servo to 90 degrees...");
    updateServo(90);
    delay(200);       // Wait for servo to return to center

    // Update radar telemetry with the fresh scanned values
    current_radar_data.left_dist = left_dist;
    current_radar_data.right_dist = right_dist;
    detectObstacle();

    // 5. Automatically turn the car in the direction of the empty space
    if (scanned_left > scanned_right + 5.0f && scanned_left > OBSTACLE_AVOID_DISTANCE_CM) {
        Serial.printf("[AUTO-AVOID] Step 5: Turning LEFT into empty space (%.1f cm clear)...\n", scanned_left);
        turnLeft(200);
        delay(420);  // Pivot turn towards empty space on left
        stopMotors();
    } else if (scanned_right > scanned_left + 5.0f && scanned_right > OBSTACLE_AVOID_DISTANCE_CM) {
        Serial.printf("[AUTO-AVOID] Step 5: Turning RIGHT into empty space (%.1f cm clear)...\n", scanned_right);
        turnRight(200);
        delay(420);  // Pivot turn towards empty space on right
        stopMotors();
    } else if (scanned_left > OBSTACLE_AVOID_DISTANCE_CM) {
        Serial.printf("[AUTO-AVOID] Step 5: Turning LEFT into empty space (%.1f cm clear)...\n", scanned_left);
        turnLeft(200);
        delay(420);
        stopMotors();
    } else if (scanned_right > OBSTACLE_AVOID_DISTANCE_CM) {
        Serial.printf("[AUTO-AVOID] Step 5: Turning RIGHT into empty space (%.1f cm clear)...\n", scanned_right);
        turnRight(200);
        delay(420);
        stopMotors();
    } else {
        // Both sides are blocked! Dead-end trap: reverse away, then turn around
        Serial.println("[AUTO-AVOID] Step 5: Dead-end! All directions blocked. Reversing away...");
        moveBackward(190);
        delay(450);  // Back away from obstacle
        stopMotors();
        delay(100);
        turnLeft(200);
        delay(380);  // Turn away from obstacle
        stopMotors();
    }

    // Refresh forward clearance measurement after turn
    center_dist = readDistance();
    current_dist = center_dist;
    current_radar_data.center_dist = center_dist;
    current_radar_data.distance = center_dist;
    detectObstacle();

    // 6. Move forward into the newly cleared empty space
    if (center_dist > OBSTACLE_AVOID_DISTANCE_CM) {
        Serial.println("[AUTO-AVOID] Step 6: Path ahead is now clear! Driving forward into empty space...");
        moveForward(180);
        delay(400);  // Drive forward into the clear space
        stopMotors();
    }

    resetSafetyWatchdog();
    Serial.println("[AUTO-AVOID] Step 7: Avoidance complete! Car safely repositioned in empty space.\n");
}
