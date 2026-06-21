#include "movement.h"

#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>
#include <cmath>

#include "collision.h"
#include "types.h"
#include "globals.h"

const float gravity    = -9.8f;
const float jump_speed =  4.0f;
const float step_height = 0.20f;   // altura máxima de degrau
const float step_subdiv = 0.04f;   // incremento de subida (suavidade)

// Helper: testa OBB contra cenário, retorna true se LIVRE (não colide com SOLID/DOOR)
static bool IsPositionFree(const glm::vec4 &pos, const glm::vec4 &halfExt, float yaw)
{
    CollisionOBB obb = {pos, halfExt, yaw};
    CollisionShapeType col = CollidesWithScenarioObb(obb, g_ScenarioCollisionShapes);
    return (col != CollisionShapeType::SOLID && col != CollisionShapeType::DOOR);
}

// Tenta movimento horizontal com step climbing incremental
// Retorna true e atualiza pos se conseguiu (possivelmente com elevação)
static bool TryMoveHorizontal(glm::vec4 &pos, float delta, int axis,
                               const glm::vec4 &halfExt, float yaw, float margin)
{
    glm::vec4 test = pos;
    test.y += margin / 2.0f;
    (axis == 0 ? test.x : test.z) += delta;

    if (IsPositionFree(test, halfExt, yaw)) {
        (axis == 0 ? pos.x : pos.z) = (axis == 0 ? test.x : test.z);
        return true;
    }

    // Sobe incrementalmente: testa degraus de step_subdiv em step_subdiv
    for (float lifted = step_subdiv; lifted <= step_height + 0.001f; lifted += step_subdiv) {
        glm::vec4 step = test;
        step.y += lifted;
        if (IsPositionFree(step, halfExt, yaw)) {
            pos.x = step.x;
            pos.y = step.y;
            pos.z = step.z;
            return true;
        }
    }
    return false;
}

float UpdatePlayerMovement(GLFWwindow *window, float delta_time)
{
    glm::vec4 horizontal_move(0.0f);
    float vertical_move_amount = 0.0f;
    float move_input = 0.0f;

    if (g_IsClimbingALadder || g_IsClimbingAVine)
    {
        g_PlayerVerticalVelocity = 0.0f;
        float climb_speed = g_IsClimbingALadder ? 2.0f : 1.5f;
        if (g_WPressed) vertical_move_amount += climb_speed * delta_time;
        if (g_SPressed) vertical_move_amount -= climb_speed * delta_time;

        if (g_IsClimbingAVine)
        {
            glm::vec4 vine_normal;
            glm::vec4 normal_test_extents = g_PlayerCubeHalfExtents * 1.1f;
            if (GetVinesNormal(g_PlayerCubePosition, normal_test_extents, g_ScenarioCollisionShapes, vine_normal))
            {
                if (std::abs(vine_normal.y) < 0.7f)
                {
                    float target_yaw = std::atan2(vine_normal.x, -vine_normal.z);
                    float diff = WrapAnglePi(target_yaw - g_PlayerYaw);
                    g_PlayerYaw += diff * std::min(1.0f, 10.0f * delta_time);
                }
            }

            float side_input = 0.0f;
            if (g_APressed) side_input += 1.0f;
            if (g_DPressed) side_input -= 1.0f;
            
            glm::vec4 player_right(std::cos(g_PlayerYaw), 0.0f, std::sin(g_PlayerYaw), 0.0f);
            horizontal_move = player_right * (side_input * 1.5f * delta_time);
        }
    }
    else
    {
        const float turn_speed = 2.1f;
        if (g_APressed) g_PlayerYaw -= turn_speed * delta_time;
        if (g_DPressed) g_PlayerYaw += turn_speed * delta_time;

        const float move_speed = g_PlayerStateMachine.GetMovementSpeed();
        const glm::vec4 player_back(std::sin(g_PlayerYaw), 0.0f, -std::cos(g_PlayerYaw), 0.0f);
        const glm::vec4 player_forward = -player_back;

        if (g_WPressed) move_input += 1.0f;
        if (g_SPressed) move_input -= 1.0f;

        horizontal_move = player_forward * (move_input * move_speed * delta_time);

        g_PlayerVerticalVelocity += gravity * delta_time;
        if (g_SpacePressed && g_PlayerOnGround)
        {
            g_PlayerVerticalVelocity = jump_speed;
            g_PlayerOnGround = false;
        }
        vertical_move_amount = g_PlayerVerticalVelocity * delta_time;
    }

    glm::vec4 updated_position = g_PlayerCubePosition;
    const float horizontal_test_margin = 0.1f;
    glm::vec4 h_half_extents = g_PlayerCubeHalfExtents;
    h_half_extents.y -= horizontal_test_margin / 2.0f;

    // --- Movimento X (com step climbing incremental) ---
    TryMoveHorizontal(updated_position, horizontal_move.x, 0,
                      h_half_extents, g_PlayerYaw, horizontal_test_margin);

    // --- Movimento Z (com step climbing incremental) ---
    TryMoveHorizontal(updated_position, horizontal_move.z, 2,
                      h_half_extents, g_PlayerYaw, horizontal_test_margin);

    // --- Movimento Y (gravidade / pulo) ---
    glm::vec4 test_pos_y = updated_position;
    test_pos_y.y += vertical_move_amount;
    
    if (IsPositionFree(test_pos_y, g_PlayerCubeHalfExtents, g_PlayerYaw))
    {
        updated_position.y = test_pos_y.y;
        if (!g_IsClimbingALadder && !g_IsClimbingAVine)
            g_PlayerOnGround = false;
    }
    else
    {
        g_PlayerVerticalVelocity = 0.0f;
        g_PlayerOnGround = true;
    }

    g_PlayerCubePosition = updated_position;

    glm::vec4 detection_extents = g_PlayerCubeHalfExtents * 1.1f;
    CollisionOBB detection_obb = {g_PlayerCubePosition, detection_extents, g_PlayerYaw};
    g_CollidedWithAVine = IsCollidingWithTypeObb(detection_obb, g_ScenarioCollisionShapes, CollisionShapeType::VINES);
    g_CollidedWithALadder = IsCollidingWithTypeObb(detection_obb, g_ScenarioCollisionShapes, CollisionShapeType::LADDER);
    
    CollisionOBB real_obb = {g_PlayerCubePosition, g_PlayerCubeHalfExtents, g_PlayerYaw};
    CollisionShapeType real_col = CollidesWithScenarioObb(real_obb, g_ScenarioCollisionShapes);
    g_PlayerCubeColliding = (real_col == CollisionShapeType::SOLID || real_col == CollisionShapeType::DOOR);

    if (g_IsClimbingAVine && !g_CollidedWithAVine) g_IsClimbingAVine = false;
    if (g_IsClimbingALadder && !g_CollidedWithALadder) g_IsClimbingALadder = false;

    glfwSetWindowTitle(
        window,
        g_PlayerCubeColliding ? "INF01047 - Colisao: DETECTADA" : "INF01047 - Colisao: livre");

    return move_input;
}
