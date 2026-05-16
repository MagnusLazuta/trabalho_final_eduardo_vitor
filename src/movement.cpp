#include "movement.h"

#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>
#include <cmath>

#include "collision.h"
#include "types.h"
#include "globals.h"

const float gravity = -9.8f;      // aceleração
const float jump_speed = 5.0f;    // pulo

float UpdatePlayerMovement(GLFWwindow* window, float delta_time)
{
    glm::vec4 intended_move(0.0f);

    const float move_speed = 3.5f;
    const float turn_speed = 2.1f;

    // rotation
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        g_PlayerYaw -= turn_speed * delta_time;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        g_PlayerYaw += turn_speed * delta_time;

    const glm::vec4 player_back(
        std::sin(g_PlayerYaw), 0.0f,
        -std::cos(g_PlayerYaw), 0.0f);

    const glm::vec4 player_forward = -player_back;

    float move_input = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        move_input += 1.0f;

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        move_input -= 1.0f;

    intended_move =
        player_forward *
        (move_input * move_speed * delta_time);

    // aplicar gravidade
    g_PlayerVerticalVelocity += gravity * delta_time;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && g_PlayerOnGround){
        g_PlayerVerticalVelocity = jump_speed;
        g_PlayerOnGround = false;
    }

    // slide movement
    glm::vec4 updated_position = g_PlayerCubePosition;

    glm::vec4 test_position_x = updated_position;
    test_position_x.x += intended_move.x;

    if (!CollidesWithScenario(
            test_position_x,
            g_ScenarioCollisionShapes,
            g_PlayerCubeHalfExtents))
    {
        updated_position.x = test_position_x.x;
    }

    glm::vec4 test_position_z = updated_position;
    test_position_z.z += intended_move.z;

    if (!CollidesWithScenario(
            test_position_z,
            g_ScenarioCollisionShapes,
            g_PlayerCubeHalfExtents))
    {
        updated_position.z = test_position_z.z;
    }

    // movimento vertical (gravidade)
    glm::vec4 test_position_y = updated_position;
    test_position_y.y += g_PlayerVerticalVelocity * delta_time;

    if (!CollidesWithScenario(
            test_position_y,
            g_ScenarioCollisionShapes,
            g_PlayerCubeHalfExtents))
    {
        updated_position.y = test_position_y.y;
        g_PlayerOnGround = false;
    }
    else
    {
        // bateu no chão ou teto
        g_PlayerVerticalVelocity = 0.0f;
        g_PlayerOnGround = true;
    }

    g_PlayerCubePosition = updated_position;

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
    return move_input;
}