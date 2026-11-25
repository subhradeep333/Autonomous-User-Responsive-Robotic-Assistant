/*
 * ESP32 0.96" I2C OLED Display (SSD1306 128x64) Driver & AURA Animation Engine
 * Displays cybernetic AURA boot sequence on startup and live HUD telemetry during driving.
 */

#include "oled_manager.h"
#include "config.h"
#include "safety_controller.h"
#include "buzzer_manager.h"

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

// 24x12 monochrome pixel-art key icon for Frame 1
static const unsigned char KEY_BMP[] PROGMEM = {
  0x3E, 0x00, 0x00,
  0x7F, 0x00, 0x00,
  0xFF, 0x80, 0x00,
  0xE7, 0x80, 0x00,
  0xC3, 0xFF, 0xFE,
  0xC3, 0xFF, 0xFF,
  0xC3, 0xFF, 0xFE,
  0xE7, 0x86, 0x66,
  0xFF, 0x86, 0x64,
  0x7F, 0x00, 0x00,
  0x3E, 0x00, 0x00,
  0x00, 0x00, 0x00
};

// Draws a 3px-thick diagonal line cleanly without pixel gaps
void drawThickLine(int x0, int y0, int x1, int y1, uint16_t color) {
  for (int i = 0; i < 3; i++) {
    display.drawLine(x0 + i, y0, x1 + i, y1, color);
  }
}

// Draws the futuristic geometric "AURA" logo (96x20 px)
// revealWidth: 0..96 controls left-to-right reveal (96 = fully visible)
void drawAuraLogo(int x, int y, int revealWidth) {
  // 'A' (chevron style, offset 0)
  drawThickLine(x,      y + 19, x + 9,  y,      SSD1306_WHITE);
  drawThickLine(x + 9,  y,      x + 18, y + 19, SSD1306_WHITE);

  // 'U' (rounded bottom, offset 25)
  int ux = x + 25;
  display.fillRoundRect(ux, y, 20, 20, 6, SSD1306_WHITE);
  display.fillRect(ux, y, 3, 7, SSD1306_WHITE);
  display.fillRect(ux + 17, y, 3, 7, SSD1306_WHITE);
  display.fillRoundRect(ux + 3, y, 14, 17, 3, SSD1306_BLACK);
  display.fillRect(ux + 3, y, 14, 6, SSD1306_BLACK);

  // 'R' (offset 50)
  int rx = x + 50;
  display.fillRoundRect(rx, y, 19, 12, 5, SSD1306_WHITE);
  display.fillRect(rx, y, 6, 12, SSD1306_WHITE);
  display.fillRoundRect(rx + 3, y + 3, 13, 6, 2, SSD1306_BLACK);
  display.fillRect(rx + 3, y + 3, 5, 6, SSD1306_BLACK);
  display.fillRect(rx, y, 3, 20, SSD1306_WHITE);
  drawThickLine(rx + 9, y + 11, rx + 17, y + 19, SSD1306_WHITE);

  // 'A' (chevron style, offset 75)
  int ax = x + 75;
  drawThickLine(ax,     y + 19, ax + 9,  y,      SSD1306_WHITE);
  drawThickLine(ax + 9, y,      ax + 18, y + 19, SSD1306_WHITE);

  // Mask unrevealed right side if animating
  if (revealWidth < 96) {
    int clamped = constrain(revealWidth, 0, 96);
    display.fillRect(x + clamped, y, 96 - clamped, 20, SSD1306_BLACK);
  }
}

// Draws subtle radiating pulse rays around the centered AURA logo (Frame 3)
void drawPulseRays() {
  // Top rays
  display.drawLine(30, 14, 24,  7, SSD1306_WHITE);
  display.drawLine(46, 12, 43,  4, SSD1306_WHITE);
  display.drawLine(64, 11, 64,  3, SSD1306_WHITE);
  display.drawLine(82, 12, 85,  4, SSD1306_WHITE);
  display.drawLine(98, 14, 104, 7, SSD1306_WHITE);

  // Bottom rays
  display.drawLine(30, 49, 24,  56, SSD1306_WHITE);
  display.drawLine(46, 51, 43,  59, SSD1306_WHITE);
  display.drawLine(64, 52, 64,  60, SSD1306_WHITE);
  display.drawLine(82, 51, 85,  59, SSD1306_WHITE);
  display.drawLine(98, 49, 104, 56, SSD1306_WHITE);

  // Side accents
  display.drawLine(10, 32,  3, 32, SSD1306_WHITE);
  display.drawLine(117, 32, 124, 32, SSD1306_WHITE);
}

// ==========================================
// FRAME 1: START / IGNITION ON...
// ==========================================
void playFrame1_Start() {
  if (!oled_detected) return;

  const int sparkleX[10] = {34, 42, 86, 94, 38, 90, 48, 80, 30, 98};
  const int sparkleY[10] = {12, 26, 14, 24, 36, 34, 10, 11, 20, 18};

  for (int step = 0; step < 24; step++) {
    playFrame1Rhythm(step);  // Continuous rhythmic electronic ignition beat

    display.clearDisplay();

    // Subtle top & bottom HUD bezel arcs
    display.drawLine(20, 1, 107, 1, SSD1306_WHITE);
    display.drawLine(10, 2, 20, 1, SSD1306_WHITE);
    display.drawLine(107, 1, 117, 2, SSD1306_WHITE);

    display.drawLine(20, 62, 107, 62, SSD1306_WHITE);
    display.drawLine(10, 61, 20, 62, SSD1306_WHITE);
    display.drawLine(107, 62, 117, 61, SSD1306_WHITE);

    // Twinkling particles around key
    for (int i = 0; i < 10; i++) {
      if ((step + i) % 3 != 0) {
        display.drawPixel(sparkleX[i], sparkleY[i], SSD1306_WHITE);
      }
    }

    // Key icon centered
    display.drawBitmap(52, 12, KEY_BMP, 24, 12, SSD1306_WHITE);

    // "START" label
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(49, 30);
    display.print(F("START"));

    // Animated "IGNITION ON..." dots
    display.setCursor(22, 48);
    display.print(F("IGNITION ON"));
    int dots = (step / 4) % 4;
    for (int d = 0; d < dots; d++) {
      display.print('.');
    }

    display.display();
    delay(75);
  }
}

// ==========================================
// FRAME 2: SYSTEM BOOT + AURA PARTICLE REVEAL
// ==========================================
void playFrame2_Boot() {
  if (!oled_detected) return;

  const int logoX = 16;
  const int logoY = 10;
  const int barX  = 14;
  const int barY  = 50;
  const int barW  = 100;
  const int barH  = 7;

  for (int pct = 0; pct <= 100; pct += 4) {
    playFrame2Rhythm(pct);  // Rhythmic ascending melodic arpeggio as progress bar fills

    display.clearDisplay();

    // Subtle corner brackets
    display.drawLine(0, 0, 8, 0, SSD1306_WHITE);
    display.drawLine(0, 0, 0, 8, SSD1306_WHITE);
    display.drawLine(127, 0, 119, 0, SSD1306_WHITE);
    display.drawLine(127, 0, 127, 8, SSD1306_WHITE);

    // Reveal AURA logo left-to-right
    int revealW = map(pct, 0, 100, 0, 96);
    drawAuraLogo(logoX, logoY, revealW);

    // Particle burst along the active reveal edge
    if (pct < 100) {
      int edgeX = logoX + revealW;
      for (int p = 0; p < 16; p++) {
        int px = edgeX + random(-8, 12);
        int py = logoY + random(-4, 24);
        if (px >= 0 && px < OLED_SCREEN_WIDTH && py >= 0 && py < 38) {
          display.drawPixel(px, py, SSD1306_WHITE);
        }
      }
    }

    // "SYSTEM BOOT..." text
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(22, 39);
    display.print(F("SYSTEM BOOT..."));

    // Progress bar frame & fill
    display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
    int fillW = map(pct, 0, 100, 0, barW - 4);
    if (fillW > 0) {
      display.fillRect(barX + 2, barY + 2, fillW, barH - 4, SSD1306_WHITE);
    }

    display.display();
    delay(65);
  }

  delay(250);
}

// ==========================================
// FRAME 3: BLINK "AURA" 3 TIMES
// ==========================================
void playFrame3_Blink3Times() {
  if (!oled_detected) return;

  const int logoX = 16;
  const int logoY = 22;

  for (int i = 0; i < 3; i++) {
    playFrame3Rhythm(i); // Audio rhythm pulses synchronized with AURA graphic pulse

    // ON with pulse rays
    display.clearDisplay();
    drawAuraLogo(logoX, logoY, 96);
    drawPulseRays();
    display.display();
    delay(320);

    // OFF
    display.clearDisplay();
    display.display();
    delay(220);
  }

  // Final steady hold on "AURA"
  display.clearDisplay();
  drawAuraLogo(logoX, logoY, 96);
  display.display();
}

void playAuraBootAnimation() {
    if (!oled_detected) {
        // OLED screen not detected or disabled: still play the continuous startup rhythm!
        playFullStartupRhythm();
        return;
    }

    Serial.println("[OLED] Playing AURA Cybernetic Key & Geometric Logo Animation...");
    randomSeed(esp_random());

    // Run the 3-frame car startup sequence
    playFrame1_Start();
    playFrame2_Boot();
    playFrame3_Blink3Times();

    // Leave the majestic steady AURA logo visible on screen during service initialization
    // The live HUD will smoothly overwrite it as soon as loop() starts.
    Serial.println("[OLED] AURA Boot Animation Complete! Services Starting...");
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

    playObstacleWarning(); // Audible alert chirps on obstacle evasion maneuver

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

    // Always restore clean normal contrast upon exiting so the display is never left solid white
    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay();
    display.drawRoundRect(2, 2, 124, 60, 4, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(18, 6);
    display.print("! ! WARNING ! !");
    display.drawLine(10, 16, 118, 16, SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(28, 22);
    display.print("OBJECT");
    display.setTextSize(2);
    display.setCursor(dir_x, 44);
    display.print(dir_label);
    display.display();
}

void showAvoidanceAnimation(const char* direction) {
    blinkObjectTurn(direction, 400);
}

#else

// Stubs when OLED is disabled in config.h
void initOLED() {}
void playAuraBootAnimation() {}
void playFrame1_Start() {}
void playFrame2_Boot() {}
void playFrame3_Blink3Times() {}
void drawAuraLogo(int x, int y, int revealWidth) {}
void updateOLEDDisplay(char cmd, uint8_t speed, float dist, const char* radar_status, const char* best_path) {}
void showAvoidanceAnimation(const char* direction) {}
void showObstacleScanningAlert() {}
void blinkObjectTurn(const char* direction, int duration_ms) {}

#endif
