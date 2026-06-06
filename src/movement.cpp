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
    glm::vec4 horizontal_move(0.0f);
    float vertical_move_amount = 0.0f;
    float move_input = 0.0f;

    if (g_IsClimbingALadder || g_IsClimbingAVine)
    {
        g_PlayerVerticalVelocity = 0.0f; // Anula a gravidade enquanto estiver escalando
        float climb_speed = g_IsClimbingALadder ? 2.0f : 1.5f;
        if (g_WPressed) vertical_move_amount += climb_speed * delta_time;
        if (g_SPressed) vertical_move_amount -= climb_speed * delta_time;

        if (g_IsClimbingAVine)
        {
            // Força o personagem a olhar para a videira (90 graus com a superfície)
            glm::vec4 vine_normal;
            // Usamos uma margem um pouco maior para a normal para garantir suavidade
            glm::vec4 normal_test_extents = g_PlayerCubeHalfExtents * 1.1f;
            if (GetVinesNormal(g_PlayerCubePosition, normal_test_extents, g_ScenarioCollisionShapes, vine_normal))
            {
                // Ignoramos normais predominantemente verticais (Y alto) para evitar girar no topo
                if (std::abs(vine_normal.y) < 0.7f) 
                {
                    float target_yaw = std::atan2(vine_normal.x, -vine_normal.z);
                    
                    // Suavização simples para evitar viradas bruscas
                    float diff = WrapAnglePi(target_yaw - g_PlayerYaw);
                    g_PlayerYaw += diff * std::min(1.0f, 10.0f * delta_time);
                }
            }

            float side_input = 0.0f;
            if (g_APressed) side_input += 1.0f; // A move para a esquerda relativa
            if (g_DPressed) side_input -= 1.0f; // D move para a direita relativa
            
            glm::vec4 player_right(std::cos(g_PlayerYaw), 0.0f, std::sin(g_PlayerYaw), 0.0f);
            horizontal_move = player_right * (side_input * 1.5f * delta_time);
        }
    }
    else
    {
        const float turn_speed = 2.1f;
        if (g_APressed) g_PlayerYaw -= turn_speed * delta_time;
        if (g_DPressed) g_PlayerYaw += turn_speed * delta_time;

        const float move_speed = 3.5f;
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

    // Teste de colisão horizontal (X e Z)
    // Movimento X
    glm::vec4 test_pos_x = updated_position;
    test_pos_x.x += horizontal_move.x;
    test_pos_x.y += horizontal_test_margin / 2.0f;
    CollisionShapeType col_x = CollidesWithScenario(test_pos_x, g_ScenarioCollisionShapes, h_half_extents);
    if (col_x != CollisionShapeType::SOLID && col_x != CollisionShapeType::DOOR)
        updated_position.x = test_pos_x.x;

    // Movimento Z
    glm::vec4 test_pos_z = updated_position;
    test_pos_z.z += horizontal_move.z;
    test_pos_z.y += horizontal_test_margin / 2.0f;
    CollisionShapeType col_z = CollidesWithScenario(test_pos_z, g_ScenarioCollisionShapes, h_half_extents);
    if (col_z != CollisionShapeType::SOLID && col_z != CollisionShapeType::DOOR)
        updated_position.z = test_pos_z.z;

    // Movimento Y
    glm::vec4 test_pos_y = updated_position;
    test_pos_y.y += vertical_move_amount;
    CollisionShapeType col_y = CollidesWithScenario(test_pos_y, g_ScenarioCollisionShapes, g_PlayerCubeHalfExtents);
    
    if (col_y != CollisionShapeType::SOLID && col_y != CollisionShapeType::DOOR)
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

    // Atualiza flags de colisão.
    glm::vec4 detection_extents = g_PlayerCubeHalfExtents * 1.1f;
    g_CollidedWithAVine = IsCollidingWithType(g_PlayerCubePosition, detection_extents, g_ScenarioCollisionShapes, CollisionShapeType::VINES);
    g_CollidedWithALadder = IsCollidingWithType(g_PlayerCubePosition, detection_extents, g_ScenarioCollisionShapes, CollisionShapeType::LADDER);
    
    CollisionShapeType real_col = CollidesWithScenario(g_PlayerCubePosition, g_ScenarioCollisionShapes, g_PlayerCubeHalfExtents);
    g_PlayerCubeColliding = (real_col == CollisionShapeType::SOLID || real_col == CollisionShapeType::DOOR);

    // Auto-stop climbing se o jogador se mover para fora da área de colisão (com a margem de segurança)
    if (g_IsClimbingAVine && !g_CollidedWithAVine) g_IsClimbingAVine = false;
    if (g_IsClimbingALadder && !g_CollidedWithALadder) g_IsClimbingALadder = false;

    printf(g_CollidedWithAVine ? "Colidiu com videira!\n" : "Não colidiu com videira.\n");
    printf(g_CollidedWithALadder ? "Colidiu com escada!\n" : "Não colidiu com escada.\n");

    glfwSetWindowTitle(
        window,
        g_PlayerCubeColliding ? "INF01047 - Colisao: DETECTADA" : "INF01047 - Colisao: livre");

    return move_input;
}
