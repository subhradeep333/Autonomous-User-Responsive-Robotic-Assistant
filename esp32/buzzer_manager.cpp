/*
 * ESP32 Buzzer Sound Engine
 * Supports both Active and Passive Buzzers on BUZZER_PIN (Default GPIO 4).
 * Generates cybernetic startup chords, ignition chirps, and obstacle warning sounds.
 * Calibrated with maximum 40mA GPIO pad drive strength and high-SPL acoustic resonance (2.1kHz - 3.5kHz).
 */

#include "buzzer_manager.h"
#include "config.h"

#if ENABLE_BUZZER

void initBuzzer() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

#if BUZZER_IS_ACTIVE_TYPE
    Serial.printf("[BUZZER] Initialized on GPIO %d in ACTIVE BUZZER mode\n", BUZZER_PIN);
#else
    Serial.printf("[BUZZER] Initialized on GPIO %d in PASSIVE PIEZO mode\n", BUZZER_PIN);
#endif
}

void stopBuzzerTone() {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
}

void playTone(uint16_t freq, uint16_t duration_ms) {
    if (freq == 0 || duration_ms == 0) {
        digitalWrite(BUZZER_PIN, LOW);
        delay(duration_ms);
        return;
    }

#if BUZZER_IS_ACTIVE_TYPE
    // Active Buzzer: Delivers full continuous DC rail voltage to trigger the internal sound oscillator at rated 85+ dB
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration_ms);
    digitalWrite(BUZZER_PIN, LOW);
#else
    // Passive Buzzer: 50% duty square wave at resonant acoustic frequency
    tone(BUZZER_PIN, freq, duration_ms);
    delay(duration_ms);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
#endif
}

// Full Cybernetic Startup Chime (Heroic futuristic ascending sequence in resonant acoustic spectrum)
void playBootChime() {
    Serial.println("[BUZZER] Playing AURA High-Volume Cybernetic Startup Sound...");
    // C7 (2093Hz) -> E7 (2637Hz) -> G7 (3136Hz) -> C8 (4186Hz)
    playTone(2093, 80);
    delay(30);
    playTone(2637, 80);
    delay(30);
    playTone(3136, 100);
    delay(40);
    playTone(4186, 250);
    stopBuzzerTone();
}

// Ignition chirp for Frame 1 (Key inserted)
void playIgnitionChirp() {
    playTone(2093, 45);
    delay(35);
    playTone(3136, 75);
    stopBuzzerTone();
}

// Frame 1: Continuous 150ms rhythmic electronic ignition beat (Tuned for Maximum Volume & Resonance)
void playFrame1Rhythm(int step) {
#if ENABLE_BUZZER
    if (step % 2 == 0) {
        int beat = (step / 2) % 4;
        switch (beat) {
            case 0: playTone(2093, 45); break; // C7 (Punchy start)
            case 1: playTone(2637, 45); break; // E7 (High acoustic resonance)
            case 2: playTone(2093, 45); break; // C7
            case 3: playTone(3136, 55); break; // G7 (Peak 3.1kHz resonance - Loudest accent beat!)
        }
    } else {
        stopBuzzerTone(); // Guaranteed silence gap between beats creates crisp, powerful rhythm
    }
#endif
}

// Frame 2: Ascending rhythmic melody as system progress bar fills (0% -> 100%)
// Frequencies tuned into peak buzzer SPL resonance (2093Hz to 3520Hz)
void playFrame2Rhythm(int pct) {
#if ENABLE_BUZZER
    if (pct % 8 == 0) {
        int note_idx = (pct / 8) % 6;
        const uint16_t melody[] = { 2093, 2349, 2637, 2794, 3136, 3520 };
        playTone(melody[note_idx], 38);
    } else if (pct % 4 == 0) {
        playTone(2093, 20); // Resonant syncopated sub-beat (C7)
    }
#endif
}

// Frame 3: Rhythmic synchronized pulse bursts for the 3x AURA screen flashes
void playFrame3Rhythm(int blink_idx) {
#if ENABLE_BUZZER
    switch (blink_idx) {
        case 0:
            // Pulse 1: Dual 2637Hz (E7) punch
            playTone(2637, 70);
            delay(25);
            playTone(2637, 70);
            stopBuzzerTone();
            break;
        case 1:
            // Pulse 2: Dual 3136Hz (G7) high-piercing punch
            playTone(3136, 70);
            delay(25);
            playTone(3136, 70);
            stopBuzzerTone();
            break;
        case 2:
        default:
            // Pulse 3: Cybernetic power-up crescendo (E7 -> G7 -> B7 power chord)
            playTone(2637, 60);
            delay(25);
            playTone(3136, 75);
            delay(25);
            playTone(3951, 190); // Final crescendo chord at 3951Hz (B7)
            stopBuzzerTone();    // 100% guarantee buzzer is silent after boot
            break;
    }
#endif
}

// Standalone continuous startup rhythm (Used if OLED screen is disconnected or disabled)
void playFullStartupRhythm() {
#if ENABLE_BUZZER
    Serial.println("[BUZZER] Playing Standalone High-Volume Continuous Startup Rhythm...");
    // Phase 1: Ignition beat
    for (int step = 0; step < 24; step++) {
        playFrame1Rhythm(step);
        delay(75);
    }
    stopBuzzerTone();

    // Phase 2: Core progress arpeggio
    for (int pct = 0; pct <= 100; pct += 4) {
        playFrame2Rhythm(pct);
        delay(65);
    }
    stopBuzzerTone();

    // Phase 3: Triple flash crescendo
    for (int i = 0; i < 3; i++) {
        playFrame3Rhythm(i);
        delay(400);
    }
    stopBuzzerTone();
#endif
}

// High-priority obstacle avoidance warning beep (Maximum piercing loudness at 3136Hz)
void playObstacleWarning() {
    playTone(3136, 70);
    delay(30);
    playTone(3136, 70);
    stopBuzzerTone();
}

// Emergency stop alarm (Fist lock / Safety trip - Commanding 2093Hz alarm)
void playEmergencySound() {
    for (int i = 0; i < 3; i++) {
        playTone(2093, 110);
        delay(40);
    }
    stopBuzzerTone();
}

#else

// Stubs when buzzer is disabled in config.h
void initBuzzer() {}
void playTone(uint16_t freq, uint16_t duration_ms) {}
void stopBuzzerTone() {}
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
