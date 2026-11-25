#ifndef BUZZER_MANAGER_H
#define BUZZER_MANAGER_H

#include <Arduino.h>

void initBuzzer();
void playTone(uint16_t freq, uint16_t duration_ms);
void playBootChime();
void playIgnitionChirp();
void playAuraPulseSound(int pulse_idx);
void playObstacleWarning();
void playEmergencySound();

#endif // BUZZER_MANAGER_H
