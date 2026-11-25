#ifndef OLED_MANAGER_H
#define OLED_MANAGER_H

#include <Arduino.h>

void initOLED();
void playAuraBootAnimation();
void updateOLEDDisplay(char cmd, uint8_t speed, float dist, const char* radar_status, const char* best_path);
void showAvoidanceAnimation(const char* direction);
void showObstacleScanningAlert();
void blinkObjectTurn(const char* direction, int duration_ms);

#endif // OLED_MANAGER_H
