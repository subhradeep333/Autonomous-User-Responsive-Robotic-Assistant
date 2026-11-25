#ifndef BUZZER_MANAGER_H
#define BUZZER_MANAGER_H

#include <Arduino.h>

void initBuzzer();
void playTone(uint16_t freq, uint16_t duration_ms);
void stopBuzzerTone();
void playBootChime();
void playIgnitionChirp();
void playAuraPulseSound(int pulse_idx);
void playFrame1Rhythm(int step);
void playFrame2Rhythm(int pct);
void playFrame3Rhythm(int blink_idx);
void playFullStartupRhythm();
void playObstacleWarning();
void playEmergencySound();

#endif // BUZZER_MANAGER_H
