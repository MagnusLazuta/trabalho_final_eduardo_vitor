#include <glm/vec4.hpp>
#include <vector>
#include "collision.h"
#include "PlayerState.h"

// Variáveis globais relacionadas ao movimento do personagem principal.
extern float g_PlayerYaw;
extern glm::vec4 g_PlayerCubePosition;
extern glm::vec4 g_PlayerCubeHalfExtents;
extern bool g_PlayerCubeColliding;
extern std::vector<CollisionShape> g_ScenarioCollisionShapes;
extern float g_PlayerVerticalVelocity;
extern bool g_PlayerOnGround;
extern PlayerStateMachine g_PlayerStateMachine;

// Flags de input para movimento
extern bool g_WPressed;
extern bool g_APressed;
extern bool g_SPressed;
extern bool g_DPressed;
extern bool g_SpacePressed;
extern bool g_ShiftPressed;
extern bool g_AttackPressed;
extern bool g_DefendPressed;
extern bool g_IsClimbingAVine;
extern bool g_IsClimbingALadder;
extern bool g_CollidedWithAVine;
extern bool g_CollidedWithALadder;
extern bool g_LockOnMovementActive;
extern glm::vec4 g_LockOnMovementForward;
extern glm::vec4 g_LockOnMovementRight;

// Sistema de combate com espada
extern bool g_SwordAttackHitActive;
extern float g_SwordAttackHitCooldown;
extern std::vector<bool> g_SwordHitEnemies;
extern int g_SwordDamage;
extern CollisionOBB g_SwordHitbox;

// Debug drawing
extern bool g_ShowDebugHitboxes;
extern bool g_ShowPlayerCoords;

// Sistema de portas
extern std::vector<DoorInstance> g_Doors;
extern bool g_EnterPressed;

// Sistema de baus
extern std::vector<ChestInstance> g_Chests;

// Sistema de cobweb (buraco no chão)
extern std::vector<CobwebInstance> g_Cobwebs;
extern float g_CobwebFallSpeedThreshold;

// Sistema de ghost ladder (escada fantasma)
extern std::vector<GhostLadderInstance> g_GhostLadders;
extern bool g_CollidedWithAGhostLadder;
extern bool g_IsClimbingAGhostLadder;
