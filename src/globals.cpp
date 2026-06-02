#include "globals.h"
#include "collision.h"



float g_PlayerYaw = 0.0f;
glm::vec4 g_PlayerCubePosition(0,0,0,1);
glm::vec4 g_PlayerCubeHalfExtents(0.5f,0.5f,0.5f,0);
bool g_PlayerCubeColliding = false;
std::vector<CollisionShape> g_ScenarioCollisionShapes;
float g_PlayerVerticalVelocity = 0.0f;
bool  g_PlayerOnGround = false;