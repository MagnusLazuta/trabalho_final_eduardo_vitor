#include <glm/vec4.hpp>
#include <vector>
#include "collision.h"


// Variáveis globais relacionadas ao movimento do personagem principal. 
extern float g_PlayerYaw;
extern glm::vec4 g_PlayerCubePosition;
extern glm::vec4 g_PlayerCubeHalfExtents;
extern bool g_PlayerCubeColliding;
extern std::vector<CollisionShape> g_ScenarioCollisionShapes;
extern float g_PlayerVerticalVelocity;
extern bool  g_PlayerOnGround;