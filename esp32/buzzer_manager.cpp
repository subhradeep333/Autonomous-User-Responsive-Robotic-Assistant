/*
 * ESP32 Buzzer Sound Engine
 * Supports both Active and Passive Buzzers on BUZZER_PIN (Default GPIO 4).
 * Generates cybernetic startup chords, ignition chirps, and obstacle warning sounds.
 */

#include "buzzer_manager.h"
#include "config.h"

#if ENABLE_BUZZER

void initBuzzer() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.printf("[BUZZER] Initialized on GPIO %d (Active & Passive mode ready)\n", BUZZER_PIN);
}

void playTone(uint16_t freq, uint16_t duration_ms) {
    if (freq == 0 || duration_ms == 0) {
        digitalWrite(BUZZER_PIN, LOW);
        delay(duration_ms);
        return;
    }

    // tone() generates frequency for passive buzzers and drives active buzzers cleanly
    tone(BUZZER_PIN, freq, duration_ms);
    delay(duration_ms);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
}

// Full Cybernetic Startup Chime (Heroic futuristic ascending sequence)
void playBootChime() {
    Serial.println("[BUZZER] Playing AURA Cybernetic Startup Sound...");
    // C6 (1046Hz) -> E6 (1318Hz) -> G6 (1568Hz) -> C7 (2093Hz)
    playTone(1046, 70);
    delay(30);
    playTone(1318, 70);
    delay(30);
    playTone(1568, 90);
    delay(40);
    playTone(2093, 220);
}

// Ignition chirp for Frame 1 (Key inserted)
void playIgnitionChirp() {
    playTone(1046, 40);
    delay(40);
    playTone(1568, 60);
}

// Synchronized sound pulse for Frame 3 (Blinking AURA logo 3 times)
void playAuraPulseSound(int pulse_idx) {
    switch (pulse_idx) {
        case 0:
            playTone(1318, 90);  // Pulse 1: E6
            break;
        case 1:
            playTone(1568, 90);  // Pulse 2: G6
            break;
        case 2:
        default:
            playTone(2093, 200); // Pulse 3: High C7 crescendo!
            break;
    }
}

// High-priority obstacle avoidance warning beep
void playObstacleWarning() {
    playTone(1760, 60);
    delay(30);
    playTone(1760, 60);
}

// Emergency stop alarm (Fist lock / Safety trip)
void playEmergencySound() {
    for (int i = 0; i < 3; i++) {
        playTone(880, 100);
        delay(40);
    }
}

#else

// Stubs when buzzer is disabled in config.h
void initBuzzer() {}
void playTone(uint16_t freq, uint16_t duration_ms) {}
void playBootChime() {}
void playIgnitionChirp() {}
void playAuraPulseSound(int pulse_idx) {}
void playObstacleWarning() {}
void playEmergencySound() {}

#endif
