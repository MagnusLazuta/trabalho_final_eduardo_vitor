#ifndef _MOVEMENT_H
#define _MOVEMENT_H

#include <GLFW/glfw3.h>

float UpdatePlayerMovement(GLFWwindow* window, float delta_time);
float WrapAnglePi(float angle);
float SmoothFollowAngle(float current, float target, float speed, float dt);

#endif // _MOVEMENT_H
