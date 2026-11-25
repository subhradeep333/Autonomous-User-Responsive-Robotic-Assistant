/*
 * ESP32 Hardware & Configuration Parameters
 * Optimized for Strong Wi-Fi Signal Strength & Ultra-Low Latency
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Wi-Fi Access Point Credentials & RF Parameters
#define WIFI_SSID "GestureCar"
#define WIFI_PASS "12345678"
#define WIFI_CHANNEL 1       // Dedicated 2.4GHz Wi-Fi Channel 1 (Low Interference)
#define MAX_AP_CLIENTS 4     // Allow up to 4 concurrent client connections

// Network IP Parameters
#define AP_IP_1 192
#define AP_IP_2 168
#define AP_IP_3 4
#define AP_IP_4 1

#define WEBSOCKET_PORT 81

// Bluetooth Classic SPP Feature Flag (0 = Wi-Fi Only [Default, <700KB], 1 = Dual Wi-Fi + Bluetooth [Requires Huge APP Partition])
#define ENABLE_BLUETOOTH_SERIAL 0

// L298N Motor Driver GPIO Pinout for ESP32
#define ENA_PIN 25  // PWM Speed Left Motors (ENA)
#define IN1_PIN 26  // Direction Left 1 (IN1)
#define IN2_PIN 27  // Direction Left 2 (IN2)

#define IN3_PIN 14  // Direction Right 1 (IN3)
#define IN4_PIN 18  // Direction Right 2 (IN4)
#define ENB_PIN 33  // PWM Speed Right Motors (ENB)

// ESP32 LEDC PWM Hardware Timer & Power Calibration Settings
#define PWM_FREQ 1000          // 1000Hz (1kHz) optimal linear switching frequency for L298N driver
#define PWM_RESOLUTION 8       // 8-bit resolution (0 - 255 linear duty cycle)
#define PWM_CHANNEL_ENA 0
#define PWM_CHANNEL_ENB 1

// Motor Power Calibration & Trim (Adjust if one motor pair is faster/stronger than the other)
#define LEFT_MOTOR_TRIM 1.0f   // Multiplier for Left Motors (ENA) - reduce (e.g. 0.85f) if car drifts right
#define RIGHT_MOTOR_TRIM 1.0f  // Multiplier for Right Motors (ENB) - reduce (e.g. 0.85f) if car drifts left
#define MIN_MOTOR_PWM 100      // Minimum PWM floor to overcome static gearbox friction cleanly

// Steering Mode:
// - PIVOT_STEERING false (Recommended for 4WD): Counter-rotation tank steer (Left reverse, Right forward).
//   Ensures maximum turning torque on all floor surfaces (carpet, tile, wood) without dragging dead wheels.
// - PIVOT_STEERING true: Inner wheel stops, outer wheel drives forward (only suitable for 2WD + caster ball).
#define PIVOT_STEERING false

// Turn Power Boost: Additional PWM added during turns to ensure enough torque on high-friction floors
#define TURN_POWER_BOOST 40    // Extra PWM added to turning maneuvers (clamped to 255)

// Motor Direction Polarity Inversion (Calibrated for Left: OUT1/OUT2, Right: OUT3/OUT4)
#define INVERT_LEFT_MOTOR  true   // Left Motors (OUT1/OUT2): true keeps Forward when IN1=LOW, IN2=HIGH
#define INVERT_RIGHT_MOTOR false  // Right Motors (OUT3/OUT4): false keeps Forward when IN3=HIGH, IN4=LOW

// Safety Watchdog Timer Threshold (ms)
#define SAFETY_TIMEOUT_MS 750

// Motor Acceleration Ramping Settings (Non-blocking Slew Rate Control)
#define ACCEL_STEP_PWM 10       // Speed step increment per tick
#define RAMP_INTERVAL_MS 5      // Smooth 5ms ramp interval (~90ms 0->180 accel)

// SG90 Servo & HC-SR04 Ultrasonic Radar Hardware Pinout
#define SERVO_PIN 13       // SG90 Servo PWM Signal Pin (Orange/Yellow wire)
#define TRIG_PIN 5         // HC-SR04 Ultrasonic Trigger Pin
#define ECHO_PIN 17        // HC-SR04 Ultrasonic Echo Pin (GPIO 17)
#define PWM_CHANNEL_SERVO 2 // LEDC Channel for SG90 Servo (50Hz, 16-bit)

// Radar & Servo Sweep Configuration Parameters
#define SERVO_MIN_ANGLE 30         // Leftmost scan angle (degrees)
#define SERVO_MAX_ANGLE 150        // Rightmost scan angle (degrees)
#define SERVO_SWEEP_STEP_DEG 5     // Angle increment per sweep step (5 deg gives 24 smooth steps)
#define SERVO_STEP_INTERVAL_MS 60  // Non-blocking servo update tick (60ms satisfies HC-SR04 ultrasonic echo settling time)
#define SERVO_CENTER_OFFSET_DEG 0  // Fine-tuning offset (-15 to +15) if servo horn is mechanically tilted

// Radar Operating Mode:
// - CONTINUOUS_RADAR_SWEEP 0 (On-Demand Search): Servo stays stationary facing straight ahead (90°).
//   When an obstacle is detected in front, the servo scans left & right to search for empty space, then the car turns.
// - CONTINUOUS_RADAR_SWEEP 1: Continuously sweeps 30° to 150°
#define CONTINUOUS_RADAR_SWEEP 0

// Print live radar distance & angle to Arduino Serial Monitor (115200 baud) for instant verification
#define RADAR_SERIAL_DEBUG 1

// Proximity Detection & Safety Thresholds (cm)
#define ULTRASONIC_MAX_RANGE_CM 200.0f  // Maximum valid sensing range (cm)
#define OBSTACLE_VERY_CLOSE_CM 10.0f    // Emergency stop / collision threshold (10cm)
#define OBSTACLE_WARNING_CM 25.0f       // Caution threshold (25cm)

// Autonomous Collision Prevention & Avoidance:
// - ENABLE_COLLISION_AUTOBRAKE 1: Auto-stops motors if obstacle < 10cm ahead while driving FORWARD
// - ENABLE_AUTONOMOUS_AVOIDANCE 1: When obstacle detected, car stops immediately and steers towards empty space
#define ENABLE_COLLISION_AUTOBRAKE 1
#define ENABLE_AUTONOMOUS_AVOIDANCE 1
#define OBSTACLE_AVOID_DISTANCE_CM 20.0f  // Distance threshold (20cm) to trigger immediate stop & avoidance maneuver

// 0.96" I2C OLED Display (SSD1306 128x64) Configuration & AURA Animations
#define ENABLE_OLED_DISPLAY 1             // 1 = Enable 0.96" OLED HUD & AURA Boot Animation, 0 = Disable
#define OLED_SDA_PIN 21                   // ESP32 I2C Data Pin (SDA)
#define OLED_SCL_PIN 22                   // ESP32 I2C Clock Pin (SCL)
#define OLED_SCREEN_WIDTH 128             // OLED display width in pixels
#define OLED_SCREEN_HEIGHT 64             // OLED display height in pixels
#define OLED_I2C_ADDRESS 0x3C             // Default I2C address for 0.96" SSD1306 (alternate 0x3D)
#define OLED_REFRESH_INTERVAL_MS 100      // Non-blocking HUD refresh tick (100ms = 10 FPS)

#endif // CONFIG_H
