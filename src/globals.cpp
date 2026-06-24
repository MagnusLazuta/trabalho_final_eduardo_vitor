#include "globals.h"
#include "collision.h"

float g_PlayerYaw = 0.0f;
glm::vec4 g_PlayerCubePosition(0, 0, 0, 1);
glm::vec4 g_PlayerCubeHalfExtents(0.5f, 0.5f, 0.5f, 0);
bool g_PlayerCubeColliding = false;
std::vector<CollisionShape> g_ScenarioCollisionShapes;
float g_PlayerVerticalVelocity = 0.0f;
bool g_PlayerOnGround = false;

bool g_WPressed = false;
bool g_APressed = false;
bool g_SPressed = false;
bool g_DPressed = false;
bool g_SpacePressed = false;
bool g_ShiftPressed = false;
bool g_AttackPressed = false;
bool g_DefendPressed = false;
bool g_IsClimbingAVine = false;
bool g_IsClimbingALadder = false;
bool g_CollidedWithAVine = false;
bool g_CollidedWithALadder = false;
bool g_LockOnMovementActive = false;
glm::vec4 g_LockOnMovementForward(0.0f, 0.0f, 1.0f, 0.0f);
glm::vec4 g_LockOnMovementRight(1.0f, 0.0f, 0.0f, 0.0f);

bool g_SwordAttackHitActive = false;
float g_SwordAttackHitCooldown = 0.0f;
std::vector<bool> g_SwordHitEnemies;
int g_SwordDamage = 1;
CollisionOBB g_SwordHitbox = {glm::vec4(0.0f), glm::vec4(0.0f), 0.0f};

bool g_ShowDebugHitboxes = false;
bool g_ShowPlayerCoords = false;

PlayerStateMachine g_PlayerStateMachine;

std::vector<DoorInstance> g_Doors;
bool g_EnterPressed = false;

std::vector<ChestInstance> g_Chests;

std::vector<CobwebInstance> g_Cobwebs;
float g_CobwebFallSpeedThreshold = -7.0f;
