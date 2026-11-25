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

// Frame 1: Continuous 150ms rhythmic electronic ignition beat
void playFrame1Rhythm(int step) {
#if ENABLE_BUZZER
    if (step % 2 == 0) {
        int beat = (step / 2) % 4;
        switch (beat) {
            case 0: tone(BUZZER_PIN, 1046, 25); break; // C6
            case 1: tone(BUZZER_PIN, 1318, 25); break; // E6
            case 2: tone(BUZZER_PIN, 1046, 25); break; // C6
            case 3: tone(BUZZER_PIN, 1568, 35); break; // G6 (Accent beat)
        }
    }
#endif
}

// Frame 2: Ascending rhythmic melody as system progress bar fills (0% -> 100%)
void playFrame2Rhythm(int pct) {
#if ENABLE_BUZZER
    if (pct % 8 == 0) {
        int note_idx = (pct / 8) % 6;
        const uint16_t melody[] = { 1046, 1175, 1318, 1568, 1760, 2093 };
        tone(BUZZER_PIN, melody[note_idx], 32);
    } else if (pct % 4 == 0) {
        tone(BUZZER_PIN, 880, 18); // Syncopated sub-beat (A5)
    }
#endif
}

// Frame 3: Rhythmic synchronized pulse bursts for the 3x AURA screen flashes
void playFrame3Rhythm(int blink_idx) {
#if ENABLE_BUZZER
    switch (blink_idx) {
        case 0:
            tone(BUZZER_PIN, 1318, 60);
            delay(70);
            tone(BUZZER_PIN, 1318, 60);
            break;
        case 1:
            tone(BUZZER_PIN, 1568, 60);
            delay(70);
            tone(BUZZER_PIN, 1568, 60);
            break;
        case 2:
        default:
            tone(BUZZER_PIN, 1760, 50);
            delay(60);
            tone(BUZZER_PIN, 2093, 70);
            delay(80);
            tone(BUZZER_PIN, 2349, 180); // Final crescendo chord!
            break;
    }
#endif
}

// Standalone continuous startup rhythm (Used if OLED screen is disconnected or disabled)
void playFullStartupRhythm() {
#if ENABLE_BUZZER
    Serial.println("[BUZZER] Playing Standalone Continuous Startup Rhythm...");
    // Phase 1: Ignition beat
    for (int step = 0; step < 24; step++) {
        playFrame1Rhythm(step);
        delay(75);
    }
    // Phase 2: Core progress arpeggio
    for (int pct = 0; pct <= 100; pct += 4) {
        playFrame2Rhythm(pct);
        delay(65);
    }
    // Phase 3: Triple flash crescendo
    for (int i = 0; i < 3; i++) {
        playFrame3Rhythm(i);
        delay(400);
    }
#endif
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
void playFrame1Rhythm(int step) {}
void playFrame2Rhythm(int pct) {}
void playFrame3Rhythm(int blink_idx) {}
void playFullStartupRhythm() {}
void playObstacleWarning() {}
void playEmergencySound() {}

#endif
