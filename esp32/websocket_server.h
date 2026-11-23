#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <Arduino.h>

void setupWebSocketServer();
void loopWebSocketServer();
void checkAutonomousObstacleAvoidance();
void setActiveCommand(char cmd);
char getActiveCommand();

#endif // WEBSOCKET_SERVER_H
