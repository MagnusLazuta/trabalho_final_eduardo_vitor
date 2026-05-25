#include "movement.h"

#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>
#include <glm/geometric.hpp>
#include <cmath>

#include "collision.h"
#include "types.h"
#include "globals.h"
#include "math_utils.h"

const float gravity = -9.8f;
const float jump_speed = 5.0f;

// =============================================================
// ESTADO PERSISTENTE DO BACKWARD TURN
// =============================================================

static bool g_BackwardTurnLocked = false;
static float g_BackwardTargetYaw = 0.0f;

float UpdatePlayerMovement(
    GLFWwindow* window,
    float delta_time,
    float g_CameraYaw)
{
    glm::vec4 intended_move(0.0f);

    const float move_speed = 3.5f;
    const float turn_speed = 6.0f;

    // =========================================================
    // INPUT WASD
    // =========================================================

    float input_x = 0.0f;
    float input_z = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        input_z += 1.0f;

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        input_z -= 1.0f;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        input_x -= 1.0f;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        input_x += 1.0f;

    bool pressing_s =
        glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;

    // =========================================================
    // DIREÇÃO RELATIVA À CÂMERA
    // =========================================================

    glm::vec4 cam_forward(
        -std::sin(g_CameraYaw),
         0.0f,
         std::cos(g_CameraYaw),
         0.0f
    );

    glm::vec4 cam_right(
         std::cos(g_CameraYaw),
         0.0f,
         std::sin(g_CameraYaw),
         0.0f
    );

    glm::vec4 move_dir =
        cam_forward * input_z +
        cam_right * input_x;

    bool has_input =
        glm::length(move_dir) > 0.001f;

    if (has_input)
    {
        move_dir = glm::normalize(move_dir);

        // =====================================================
        // TARGET YAW
        // =====================================================

        float target_yaw;

        // -----------------------------------------------------
        // BACKWARD LOCK
        // -----------------------------------------------------

        if (pressing_s &&
            input_z < 0.0f &&
            std::abs(input_x) < 0.001f)
        {
            // primeira frame apertando S
            if (!g_BackwardTurnLocked)
            {
                glm::vec4 backward_dir = -cam_forward;

                g_BackwardTargetYaw =
                    std::atan2(
                        -backward_dir.x,
                         backward_dir.z
                    );

                g_BackwardTargetYaw =
                    WrapAnglePi(g_BackwardTargetYaw);

                g_BackwardTurnLocked = true;
            }

            target_yaw = g_BackwardTargetYaw;
        }
        else
        {
            g_BackwardTurnLocked = false;

            target_yaw =
                std::atan2(
                    -move_dir.x,
                     move_dir.z
                );

            target_yaw =
                WrapAnglePi(target_yaw);
        }

        // =====================================================
        // ROTAÇÃO SUAVE
        // =====================================================

        float angle_diff =
            WrapAnglePi(target_yaw - g_PlayerYaw);

        g_PlayerYaw +=
            angle_diff *
            turn_speed *
            delta_time;

        g_PlayerYaw =
            WrapAnglePi(g_PlayerYaw);

        // =====================================================
        // MOVIMENTO
        // =====================================================

        intended_move =
            move_dir *
            (move_speed * delta_time);
    }

    // =========================================================
    // GRAVIDADE
    // =========================================================

    g_PlayerVerticalVelocity +=
        gravity * delta_time;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS &&
        g_PlayerOnGround)
    {
        g_PlayerVerticalVelocity = jump_speed;
        g_PlayerOnGround = false;
    }

    // =========================================================
    // MOVIMENTO COM SLIDE NAS COLISÕES
    // =========================================================

    glm::vec4 updated_position =
        g_PlayerCubePosition;

    const float horizontal_test_margin = 0.1f;

    glm::vec4 h_half_extents =
        g_PlayerCubeHalfExtents;

    h_half_extents.y -=
        horizontal_test_margin / 2.0f;

    // ---------------------------------------------------------
    // X
    // ---------------------------------------------------------

    glm::vec4 test_position_x =
        updated_position;

    test_position_x.x += intended_move.x;
    test_position_x.y +=
        horizontal_test_margin / 2.0f;

    if (!CollidesWithScenario(
            test_position_x,
            g_ScenarioCollisionShapes,
            h_half_extents))
    {
        updated_position.x =
            test_position_x.x;
    }

    // ---------------------------------------------------------
    // Z
    // ---------------------------------------------------------

    glm::vec4 test_position_z =
        updated_position;

    test_position_z.z += intended_move.z;
    test_position_z.y +=
        horizontal_test_margin / 2.0f;

    if (!CollidesWithScenario(
            test_position_z,
            g_ScenarioCollisionShapes,
            h_half_extents))
    {
        updated_position.z =
            test_position_z.z;
    }

    // ---------------------------------------------------------
    // Y
    // ---------------------------------------------------------

    glm::vec4 test_position_y =
        updated_position;

    test_position_y.y +=
        g_PlayerVerticalVelocity *
        delta_time;

    if (!CollidesWithScenario(
            test_position_y,
            g_ScenarioCollisionShapes,
            g_PlayerCubeHalfExtents))
    {
        updated_position.y =
            test_position_y.y;

        g_PlayerOnGround = false;
    }
    else
    {
        g_PlayerVerticalVelocity = 0.0f;
        g_PlayerOnGround = true;
    }

    // =========================================================
    // ATUALIZA ESTADO FINAL
    // =========================================================

    g_PlayerCubePosition =
        updated_position;

    g_PlayerCubeColliding =
        CollidesWithScenario(
            g_PlayerCubePosition,
            g_ScenarioCollisionShapes,
            g_PlayerCubeHalfExtents);

    glfwSetWindowTitle(
        window,
        g_PlayerCubeColliding ?
        "INF01047 - Colisao: DETECTADA" :
        "INF01047 - Colisao: livre");

    return has_input ? 1.0f : 0.0f;
}