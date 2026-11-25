/*
 * ESP32 0.96" I2C OLED Display (SSD1306 128x64) Driver & AURA Animation Engine
 * Displays cybernetic AURA boot sequence on startup and live HUD telemetry during driving.
 */

#include "oled_manager.h"
#include "config.h"
#include "safety_controller.h"

#if ENABLE_OLED_DISPLAY
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, -1);
static bool oled_detected = false;
static unsigned long last_oled_refresh = 0;

void initOLED() {
    Serial.println("[OLED] Initializing 0.96\" I2C OLED (SSD1306)...");
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    Wire.setClock(400000);  // 400kHz Fast I2C mode

    // Try primary I2C address (0x3C)
    if (display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        oled_detected = true;
        Serial.printf("[OLED] Display detected at address 0x%02X\n", OLED_I2C_ADDRESS);
    } else if (display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
        // Fallback to secondary I2C address (0x3D)
        oled_detected = true;
        Serial.println("[OLED] Display detected at secondary address 0x3D");
    } else {
        oled_detected = false;
        Serial.println("[!] OLED display not found on I2C bus (Check SDA=21, SCL=22, VCC=3.3V/5V, GND). Skipping OLED.");
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

void playAuraBootAnimation() {
    if (!oled_detected) return;

    Serial.println("[OLED] Playing AURA Cybernetic Boot Animation...");

    // -------------------------------------------------------------
    // SCENE 1: Expanding Radiant AURA Energy Rings
    // -------------------------------------------------------------
    for (int r = 4; r <= 38; r += 6) {
        display.clearDisplay();
        
        // Draw expanding concentric energy rings
        display.drawCircle(64, 32, r, SSD1306_WHITE);
        if (r > 10) display.drawCircle(64, 32, r - 6, SSD1306_WHITE);
        if (r > 20) display.drawCircle(64, 32, r - 16, SSD1306_WHITE);

        // Core pulsating orb
        display.fillCircle(64, 32, 3, SSD1306_WHITE);

        // Corner futuristic crosshairs
        display.drawLine(2, 2, 8, 2, SSD1306_WHITE);
        display.drawLine(2, 2, 2, 8, SSD1306_WHITE);
        display.drawLine(125, 2, 119, 2, SSD1306_WHITE);
        display.drawLine(125, 2, 125, 8, SSD1306_WHITE);
        display.drawLine(2, 61, 8, 61, SSD1306_WHITE);
        display.drawLine(2, 61, 2, 55, SSD1306_WHITE);
        display.drawLine(125, 61, 119, 61, SSD1306_WHITE);
        display.drawLine(125, 61, 125, 55, SSD1306_WHITE);

        display.display();
        delay(60);
    }

    // -------------------------------------------------------------
    // SCENE 2: Cyber Text Reveal of "A U R A" Title Banner
    // -------------------------------------------------------------
    display.clearDisplay();
    // High-tech bounding frame
    display.drawRoundRect(4, 4, 120, 56, 4, SSD1306_WHITE);
    display.drawRoundRect(6, 6, 116, 52, 3, SSD1306_WHITE);

    // Glowing main title
    display.setTextSize(2);
    display.setCursor(20, 14);
    display.print("A U R A");

    // Subtitle
    display.setTextSize(1);
    display.setCursor(16, 34);
    display.print("ROBOTIC ASSISTANT");
    display.setCursor(24, 45);
    display.print("AI GESTURE CAR");

    display.display();
    delay(750);

    // Flash inverted effect for dramatic cyber impact
    display.invertDisplay(true);
    delay(100);
    display.invertDisplay(false);
    delay(300);

    // -------------------------------------------------------------
    // SCENE 3: System Diagnostic & High-Tech Progress Bar
    // -------------------------------------------------------------
    const char* boot_steps[] = {
        "INIT HARDWARE...",
        "CALIBRATING PWM...",
        "RADAR SENSORS OK",
        "WIFI AP ONLINE",
        "SYSTEM READY"
    };

    for (int p = 0; p <= 100; p += 20) {
        display.clearDisplay();

        // Title header
        display.setTextSize(1);
        display.setCursor(38, 6);
        display.print("* A U R A *");
        display.drawLine(10, 16, 118, 16, SSD1306_WHITE);

        // Status text
        int step_idx = p / 20;
        if (step_idx > 4) step_idx = 4;
        display.setCursor(14, 25);
        display.print(boot_steps[step_idx]);

        // Progress bar container
        display.drawRoundRect(14, 38, 100, 10, 2, SSD1306_WHITE);
        // Fill progress
        int fill_width = (p * 96) / 100;
        if (fill_width > 0) {
            display.fillRect(16, 40, fill_width, 6, SSD1306_WHITE);
        }

        // Percentage readout
        display.setCursor(52, 51);
        display.print(p);
        display.print("%");

        display.display();
        delay(120);
    }

    delay(400);
    display.clearDisplay();
    display.display();
    Serial.println("[OLED] AURA Boot Animation Complete! Live HUD Active.");
}

void updateOLEDDisplay(char cmd, uint8_t speed, float dist, const char* radar_status, const char* best_path) {
    if (!oled_detected) return;

    unsigned long now = millis();
    if (now - last_oled_refresh < OLED_REFRESH_INTERVAL_MS) {
        return;  // Throttle display refreshes to ~10 FPS to save CPU
    }
    last_oled_refresh = now;

    display.clearDisplay();

    // -------------------------------------------------------------
    // TOP STATUS BAR (Y: 0 to 14)
    // -------------------------------------------------------------
    display.setTextSize(1);
    display.setCursor(2, 2);
    display.print("AP:GestureCar");

    // Battery Indicator (Graphic)
    display.drawRect(102, 2, 18, 9, SSD1306_WHITE);
    display.fillRect(120, 4, 2, 5, SSD1306_WHITE);
    display.fillRect(104, 4, 12, 5, SSD1306_WHITE);  // 3 bars (Good battery)

    display.drawLine(0, 13, 127, 13, SSD1306_WHITE);

    // -------------------------------------------------------------
    // CENTER DASHBOARD: COMMAND & MOTION STATE (Y: 16 to 48)
    // -------------------------------------------------------------
    bool object_detected = (dist < OBSTACLE_AVOID_DISTANCE_CM || 
                            strcmp(radar_status, "VERY CLOSE") == 0 || 
                            strcmp(radar_status, "OBSTACLE DETECTED") == 0);

    if (object_detected) {
        // High-visibility blinking "OBJECT" alert with turning direction
        bool blink_phase = ((millis() / 250) % 2 == 0);

        const char* turn_dir_text = "<<< TURN LEFT <<<";
        if (strcmp(best_path, "RIGHT") == 0) {
            turn_dir_text = ">>> TURN RIGHT >>>";
        } else if (strcmp(best_path, "BACKWARD") == 0 || strcmp(best_path, "BACK") == 0) {
            turn_dir_text = "vvv REVERSE vvv";
        }

        if (blink_phase) {
            // Filled high-contrast alert box
            display.fillRect(2, 16, 124, 32, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);

            // Bold Size 2 OBJECT
            display.setTextSize(2);
            display.setCursor(28, 18);
            display.print("OBJECT");

            // Size 1 Turning Direction
            display.setTextSize(1);
            display.setCursor(10, 36);
            display.print(turn_dir_text);
        } else {
            // Outline alert box with glowing text
            display.setTextColor(SSD1306_WHITE);
            display.drawRect(2, 16, 124, 32, SSD1306_WHITE);

            // Bold Size 2 OBJECT
            display.setTextSize(2);
            display.setCursor(28, 18);
            display.print("OBJECT");

            // Size 1 Turning Direction
            display.setTextSize(1);
            display.setCursor(10, 36);
            display.print(turn_dir_text);
        }
        display.setTextColor(SSD1306_WHITE);
    } else {
        switch (cmd) {
            case 'F':
                // Large Glowing Upward Arrow
                display.fillTriangle(24, 20, 14, 36, 34, 36, SSD1306_WHITE);
                display.fillRect(20, 36, 8, 8, SSD1306_WHITE);
                display.setTextSize(2);
                display.setCursor(44, 24);
                display.print("FORWARD");
                break;

            case 'B':
                // Downward Arrow
                display.fillTriangle(24, 44, 14, 28, 34, 28, SSD1306_WHITE);
                display.fillRect(20, 20, 8, 8, SSD1306_WHITE);
                display.setTextSize(2);
                display.setCursor(44, 24);
                display.print("REVERSE");
                break;

            case 'L':
                // Left Arrow
                display.fillTriangle(14, 32, 30, 20, 30, 44, SSD1306_WHITE);
                display.fillRect(30, 28, 8, 8, SSD1306_WHITE);
                display.setTextSize(2);
                display.setCursor(44, 24);
                display.print("LEFT");
                break;

            case 'R':
                // Right Arrow
                display.fillTriangle(38, 32, 22, 20, 22, 44, SSD1306_WHITE);
                display.fillRect(14, 28, 8, 8, SSD1306_WHITE);
                display.setTextSize(2);
                display.setCursor(48, 24);
                display.print("RIGHT");
                break;

            case 'E':
                // Emergency Stop Octagon
                display.drawRoundRect(10, 18, 108, 28, 4, SSD1306_WHITE);
                display.setTextSize(1);
                display.setCursor(20, 22);
                display.print("EMERGENCY STOP");
                display.setTextSize(1);
                display.setCursor(32, 34);
                display.print("[FIST LOCK]");
                break;

            case 'S':
            default:
                // Standby / Parked State
                display.drawRect(18, 24, 14, 14, SSD1306_WHITE);
                display.fillRect(22, 28, 6, 6, SSD1306_WHITE);
                display.setTextSize(2);
                display.setCursor(42, 24);
                display.print("STOPPED");
                break;
        }
    }

    // -------------------------------------------------------------
    // BOTTOM RADAR & OBSTACLE STATUS (Y: 50 to 63)
    // -------------------------------------------------------------
    display.drawLine(0, 49, 127, 49, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(2, 53);
    
    if (dist < 200.0f) {
        display.printf("DIST:%4.1fcm", dist);
    } else {
        display.print("DIST: CLEAR ");
    }

    // Right-aligned status indicator
    display.setCursor(82, 53);
    if (strcmp(radar_status, "VERY CLOSE") == 0) {
        display.print("!CLOSE!");
    } else if (strcmp(radar_status, "OBSTACLE DETECTED") == 0) {
        display.print("!WARN!");
    } else {
        display.printf("> %s", best_path);
    }

    display.display();
}

void showObstacleScanningAlert() {
    if (!oled_detected) return;

    display.clearDisplay();
    display.drawRoundRect(2, 2, 124, 60, 4, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(18, 6);
    display.print("! ! WARNING ! !");
    display.drawLine(10, 16, 118, 16, SSD1306_WHITE);

    // Big bold "OBJECT"
    display.setTextSize(2);
    display.setCursor(28, 22);
    display.print("OBJECT");

    display.setTextSize(1);
    display.setCursor(6, 44);
    display.print("SEARCHING CLEAR PATH");

    display.display();
}

void blinkObjectTurn(const char* direction, int duration_ms) {
    if (!oled_detected) {
        delay(duration_ms);
        return;
    }

    unsigned long start = millis();
    const unsigned long blink_interval = 125;  // 8 Hz fast eye-catching visual blink
    bool phase = false;

    // Format direction string & horizontal alignment
    const char* dir_label = "<-- LEFT";
    int dir_x = 16;
    if (strcmp(direction, "RIGHT") == 0) {
        dir_label = "RIGHT -->";
        dir_x = 10;
    } else if (strcmp(direction, "BACK") == 0 || strcmp(direction, "BACKWARD") == 0) {
        dir_label = "v REVERSE v";
        dir_x = 8;
    }

    while (millis() - start < (unsigned long)duration_ms) {
        display.clearDisplay();

        if (phase) {
            // Inverted full-screen flash: solid white background with black bold text
            display.fillRect(0, 0, 128, 64, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);

            display.setTextSize(1);
            display.setCursor(18, 6);
            display.print("! ! WARNING ! !");
            display.drawLine(10, 16, 118, 16, SSD1306_BLACK);

            // Big Blinking "OBJECT"
            display.setTextSize(2);
            display.setCursor(28, 22);
            display.print("OBJECT");

            // Turning Direction
            display.setTextSize(2);
            display.setCursor(dir_x, 44);
            display.print(dir_label);
        } else {
            // Normal high contrast: black background with white glowing text
            display.setTextColor(SSD1306_WHITE);
            display.drawRoundRect(2, 2, 124, 60, 4, SSD1306_WHITE);

            display.setTextSize(1);
            display.setCursor(18, 6);
            display.print("! ! WARNING ! !");
            display.drawLine(10, 16, 118, 16, SSD1306_WHITE);

            // Big Blinking "OBJECT"
            display.setTextSize(2);
            display.setCursor(28, 22);
            display.print("OBJECT");

            // Turning Direction
            display.setTextSize(2);
            display.setCursor(dir_x, 44);
            display.print(dir_label);
        }

        display.display();
        phase = !phase;
        resetSafetyWatchdog();

        unsigned long elapsed = millis() - start;
        unsigned long remaining = (elapsed < (unsigned long)duration_ms) ? ((unsigned long)duration_ms - elapsed) : 0;
        unsigned long sleep_time = (remaining < blink_interval) ? remaining : blink_interval;
        if (sleep_time > 0) {
            delay(sleep_time);
        }
    }

    display.setTextColor(SSD1306_WHITE);
}

void showAvoidanceAnimation(const char* direction) {
    blinkObjectTurn(direction, 400);
}

#else

// Stubs when OLED is disabled in config.h
void initOLED() {}
void playAuraBootAnimation() {}
void updateOLEDDisplay(char cmd, uint8_t speed, float dist, const char* radar_status, const char* best_path) {}
void showAvoidanceAnimation(const char* direction) {}
void showObstacleScanningAlert() {}
void blinkObjectTurn(const char* direction, int duration_ms) {}

#endif
