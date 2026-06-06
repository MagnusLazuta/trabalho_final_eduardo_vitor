#include "movement.h"

#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>
#include <cmath>

#include "collision.h"
#include "types.h"
#include "globals.h"

const float gravity = -9.8f;   // aceleração
const float jump_speed = 5.0f; // pulo

float UpdatePlayerMovement(GLFWwindow *window, float delta_time)
{
    if (g_IsClimbingALadder)
    {
        g_PlayerVerticalVelocity = 0.0f; // Anula a gravidade enquanto estiver escalando
        if (g_WPressed)
            g_PlayerCubePosition.y += 2.0f * delta_time; // Sobe
        if (g_SPressed)
            g_PlayerCubePosition.y -= 2.0f * delta_time; // Desce

        return 0.0f; // Sem movimento horizontal enquanto escalando
    }
    else if (g_IsClimbingAVine)
    {
        g_PlayerVerticalVelocity = 0.0f; // Anula a gravidade enquanto estiver escalando
        if (g_WPressed)
            g_PlayerCubePosition.y += 1.5f * delta_time; // Sobe
        if (g_SPressed)
            g_PlayerCubePosition.y -= 1.5f * delta_time; // Desce
        if (g_APressed)
            g_PlayerCubePosition.x -= 1.5f * delta_time; // Move para a esquerda
        if (g_DPressed)
            g_PlayerCubePosition.x += 1.5f * delta_time; // Move para a direita

        return 0.0f; // Sem movimento horizontal enquanto escalando
    }

    glm::vec4 intended_move(0.0f);

    const float move_speed = 3.5f;
    const float turn_speed = 2.1f;

    // rotation
    if (g_APressed)
        g_PlayerYaw -= turn_speed * delta_time;

    if (g_DPressed)
        g_PlayerYaw += turn_speed * delta_time;

    const glm::vec4 player_back(
        std::sin(g_PlayerYaw), 0.0f,
        -std::cos(g_PlayerYaw), 0.0f);

    const glm::vec4 player_forward = -player_back;

    float move_input = 0.0f;

    if (g_WPressed)
        move_input += 1.0f;

    if (g_SPressed)
        move_input -= 1.0f;

    intended_move =
        player_forward *
        (move_input * move_speed * delta_time);

    // aplicar gravidade
    g_PlayerVerticalVelocity += gravity * delta_time;

    if (g_SpacePressed && g_PlayerOnGround)
    {
        g_PlayerVerticalVelocity = jump_speed;
        g_PlayerOnGround = false;
    }

    // slide movement
    glm::vec4 updated_position = g_PlayerCubePosition;

    // Altura de "degrau" ou margem para evitar que o chão bloqueie o movimento horizontal
    const float horizontal_test_margin = 0.1f;
    glm::vec4 h_half_extents = g_PlayerCubeHalfExtents;
    h_half_extents.y -= horizontal_test_margin / 2.0f;

    glm::vec4 test_position_x = updated_position;
    test_position_x.x += intended_move.x;
    test_position_x.y += horizontal_test_margin / 2.0f;

    CollisionShapeType collision_type_x = CollidesWithScenario(
        test_position_x,
        g_ScenarioCollisionShapes,
        h_half_extents);

    if (collision_type_x == CollisionShapeType::NONE)
    {
        updated_position.x = test_position_x.x;
    }

    glm::vec4 test_position_z = updated_position;
    test_position_z.z += intended_move.z;
    test_position_z.y += horizontal_test_margin / 2.0f;

    CollisionShapeType collision_type_z = CollidesWithScenario(
        test_position_z,
        g_ScenarioCollisionShapes,
        h_half_extents);

    if (collision_type_z == CollisionShapeType::NONE)
    {
        updated_position.z = test_position_z.z;
    }

    // movimento vertical (gravidade)
    glm::vec4 test_position_y = updated_position;
    test_position_y.y += g_PlayerVerticalVelocity * delta_time;

    CollisionShapeType collision_type_y = CollidesWithScenario(
        test_position_y,
        g_ScenarioCollisionShapes,
        g_PlayerCubeHalfExtents);

    if (collision_type_y == CollisionShapeType::NONE)
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

    if (collision_type_x == CollisionShapeType::VINES || collision_type_y == CollisionShapeType::VINES || collision_type_z == CollisionShapeType::VINES)
    {
        g_CollidedWithAVine = true;
    }
    else if (collision_type_x == CollisionShapeType::LADDER || collision_type_y == CollisionShapeType::LADDER || collision_type_z == CollisionShapeType::LADDER)
    {
        g_CollidedWithALadder = true;
    }
    else
    {
        g_CollidedWithAVine = false;
        g_CollidedWithALadder = false;
    }

    printf(g_CollidedWithAVine ? "Colidiu com videira!\n" : "Não colidiu com videira.\n");
    printf(g_CollidedWithALadder ? "Colidiu com escada!\n" : "Não colidiu com escada.\n");

    glfwSetWindowTitle(
        window,
        g_PlayerCubeColliding ? "INF01047 - Colisao: DETECTADA" : "INF01047 - Colisao: livre");

    return move_input;
}