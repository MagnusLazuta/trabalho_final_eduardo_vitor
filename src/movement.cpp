#include "movement.h"

#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>
#include <cmath>

#include "collision.h"
#include "globals.h"
#include "types.h"

bool QueryBlockingEnemyCollision(const glm::vec4 &center, const glm::vec4 &half_extents);

const float gravity = -9.8f;
const float jump_speed = 4.0f;
const float step_height = 0.20f;
const float step_subdiv = 0.04f;

static bool IsCobwebBroken(const CollisionShape &shape)
{
    glm::vec4 center = (shape.bbox_min + shape.bbox_max) * 0.5f;
    for (const auto &cw : g_Cobwebs)
    {
        if (!cw.broken)
            continue;
        float dx = cw.bbox_center.x - center.x;
        float dz = cw.bbox_center.z - center.z;
        if (dx * dx + dz * dz < 0.01f)
            return true;
    }
    return false;
}

// Helper: testa OBB contra cenário, retorna true se LIVRE
// Verifica TODAS as shapes — não para no primeiro hit.
// Isso é necessário porque CollidesWithScenarioObb retorna apenas o primeiro tipo,
// mas uma posição pode colidir com VINE e SOLID simultaneamente.
static bool IsPositionFree(const glm::vec4 &pos, const glm::vec4 &halfExt, float yaw)
{
    CollisionOBB obb = {pos, halfExt, yaw};
    const CollisionAABB obb_aabb = ComputeObbAabb(obb);

    for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
    {
        const CollisionShape &shape = g_ScenarioCollisionShapes[i];
        if (shape.type != CollisionShapeType::SOLID &&
            shape.type != CollisionShapeType::DOOR &&
            shape.type != CollisionShapeType::WATER &&
            shape.type != CollisionShapeType::COBWEB_FLOORHOLE)
            continue;

        if (shape.type == CollisionShapeType::COBWEB_FLOORHOLE && IsCobwebBroken(shape))
            continue;

        CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
        if (!AabbAabbIntersect(obb_aabb, shape_aabb))
            continue;

        for (size_t t = 0; t < shape.triangles.size(); ++t)
        {
            if (TriangleIntersectsObb(shape.triangles[t], obb))
                return false;
        }
    }
    if (QueryBlockingEnemyCollision(pos, halfExt))
        return false;
    return true;
}

// Helper: como IsPositionFree mas ignora portas abertas (para movimento horizontal)
static bool IsDoorShapeOpen(const CollisionShape &shape)
{
    glm::vec4 center = (shape.bbox_min + shape.bbox_max) * 0.5f;
    for (const auto &door : g_Doors)
    {
        if (door.state == DoorState::CLOSED)
            continue;
        float dx = door.bbox_center.x - center.x;
        float dz = door.bbox_center.z - center.z;
        if (dx * dx + dz * dz < 0.01f)
            return true;
    }
    return false;
}

static bool IsPositionFreeWalkThrough(const glm::vec4 &pos, const glm::vec4 &halfExt, float yaw)
{
    CollisionOBB obb = {pos, halfExt, yaw};
    const CollisionAABB obb_aabb = ComputeObbAabb(obb);

    for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
    {
        const CollisionShape &shape = g_ScenarioCollisionShapes[i];
        if (shape.type == CollisionShapeType::NONE)
            continue;
        if (shape.type == CollisionShapeType::DOOR && IsDoorShapeOpen(shape))
            continue;
        if (shape.type == CollisionShapeType::COBWEB_FLOORHOLE && IsCobwebBroken(shape))
            continue;

        CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
        if (!AabbAabbIntersect(obb_aabb, shape_aabb))
            continue;

        for (size_t t = 0; t < shape.triangles.size(); ++t)
        {
            if (TriangleIntersectsObb(shape.triangles[t], obb))
                return false;
        }
    }
    if (QueryBlockingEnemyCollision(pos, halfExt))
        return false;
    return true;
}

// Helper para escalada: ignora colisão com VINES/LADDER (permite subir nelas)
static bool IsPositionFreeForClimbing(const glm::vec4 &pos, const glm::vec4 &halfExt, float yaw)
{
    CollisionOBB obb = {pos, halfExt, yaw};
    const CollisionAABB obb_aabb = ComputeObbAabb(obb);

    for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
    {
        const CollisionShape &shape = g_ScenarioCollisionShapes[i];
        if (shape.type == CollisionShapeType::VINES || shape.type == CollisionShapeType::LADDER)
            continue;

        CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
        if (!AabbAabbIntersect(obb_aabb, shape_aabb))
            continue;

        for (size_t t = 0; t < shape.triangles.size(); ++t)
        {
            if (TriangleIntersectsObb(shape.triangles[t], obb))
                return false;
        }
    }
    if (QueryBlockingEnemyCollision(pos, halfExt))
        return false;
    return true;
}

// Tenta movimento horizontal com step climbing incremental
static bool TryMoveHorizontal(glm::vec4 &pos, float delta, int axis,
                              const glm::vec4 &halfExt, float yaw, float margin)
{
    float original_y = pos.y;

    glm::vec4 test = pos;
    (axis == 0 ? test.x : test.z) += delta;

    if (IsPositionFreeWalkThrough(test, halfExt, yaw))
    {
        (axis == 0 ? pos.x : pos.z) = (axis == 0 ? test.x : test.z);
        return true;
    }

    for (float lifted = step_subdiv; lifted <= step_height + 0.001f; lifted += step_subdiv)
    {
        glm::vec4 step = test;
        step.y = original_y + lifted;
        if (IsPositionFreeWalkThrough(step, halfExt, yaw))
        {
            pos.x = step.x;
            pos.z = step.z;
            pos.y = step.y;
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
        if (g_WPressed)
            vertical_move_amount += climb_speed * delta_time;
        if (g_SPressed)
            vertical_move_amount -= climb_speed * delta_time;

        static int climbDebugCounter = 0;
        climbDebugCounter++;
        if (climbDebugCounter % 60 == 0)
        {
            printf("[CLIMB MOVE] type=%s vertical=%.3f pos=(%.1f,%.1f,%.1f) W=%d S=%d\n",
                   g_IsClimbingALadder ? "LADDER" : "VINE",
                   vertical_move_amount,
                   g_PlayerCubePosition.x, g_PlayerCubePosition.y, g_PlayerCubePosition.z,
                   g_WPressed, g_SPressed);
        }

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
            if (g_APressed)
                side_input += 1.0f;
            if (g_DPressed)
                side_input -= 1.0f;

            glm::vec4 player_right(std::cos(g_PlayerYaw), 0.0f, std::sin(g_PlayerYaw), 0.0f);
            horizontal_move = player_right * (side_input * 1.5f * delta_time);
        }
    }
    else
    {
        const float move_speed = g_PlayerStateMachine.GetMovementSpeed();
        if (g_LockOnMovementActive)
        {
            float forward_input = 0.0f;
            float strafe_input = 0.0f;
            if (g_WPressed)
                forward_input += 1.0f;
            if (g_SPressed)
                forward_input -= 1.0f;
            if (g_DPressed)
                strafe_input -= 1.0f;
            if (g_APressed)
                strafe_input += 1.0f;

            glm::vec4 move_direction =
                g_LockOnMovementForward * forward_input +
                g_LockOnMovementRight * strafe_input;
            move_direction.y = 0.0f;
            move_direction.w = 0.0f;

            const float move_length = std::sqrt(move_direction.x * move_direction.x +
                                                move_direction.z * move_direction.z);
            if (move_length > 0.001f)
            {
                move_direction = move_direction / move_length;
                horizontal_move = move_direction * (move_speed * delta_time);
            }

            move_input = forward_input;
        }
        else
        {
            const float turn_speed = 2.1f;
            if (g_APressed)
                g_PlayerYaw -= turn_speed * delta_time;
            if (g_DPressed)
                g_PlayerYaw += turn_speed * delta_time;

            const glm::vec4 player_back(std::sin(g_PlayerYaw), 0.0f, -std::cos(g_PlayerYaw), 0.0f);
            const glm::vec4 player_forward = -player_back;

            if (g_WPressed)
                move_input += 1.0f;
            if (g_SPressed)
                move_input -= 1.0f;
            horizontal_move = player_forward * (move_input * move_speed * delta_time);
        }

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

    // --- Movimento X ---
    TryMoveHorizontal(updated_position, horizontal_move.x, 0,
                      h_half_extents, g_PlayerYaw, horizontal_test_margin);

    // --- Movimento Z ---
    TryMoveHorizontal(updated_position, horizontal_move.z, 2,
                      h_half_extents, g_PlayerYaw, horizontal_test_margin);

    // --- Movimento Y ---
    glm::vec4 test_pos_y = updated_position;
    test_pos_y.y += vertical_move_amount;

    bool yFree;
    if (g_IsClimbingALadder || g_IsClimbingAVine)
    {
        glm::vec4 climb_extents(0.1f, 0.1f, 0.1f, 0.0f);
        yFree = IsPositionFreeForClimbing(test_pos_y, climb_extents, g_PlayerYaw);
    }
    else
        yFree = IsPositionFree(test_pos_y, g_PlayerCubeHalfExtents, g_PlayerYaw);

    if (!yFree)
    {
        CollisionOBB test_obb = {test_pos_y, g_PlayerCubeHalfExtents, g_PlayerYaw};
        CollisionShapeType block_col = CollidesWithScenarioObb(test_obb, g_ScenarioCollisionShapes);

        // Debug: log quando o jogador colide com algo caindo
        if (g_PlayerVerticalVelocity < -0.1f)
        {
            printf("[COBWEB DEBUG] yFree=false velY=%.2f block_col=%d pos_y=%.2f test_y=%.2f\n",
                   g_PlayerVerticalVelocity, (int)block_col,
                   updated_position.y, test_pos_y.y);
        }

        if (block_col == CollisionShapeType::DOOR)
        {
            CollisionAABB obb_aabb = ComputeObbAabb(test_obb);
            for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
            {
                const auto &cs = g_ScenarioCollisionShapes[i];
                if (cs.type != CollisionShapeType::DOOR)
                    continue;
                CollisionAABB shape_aabb = {cs.bbox_min, cs.bbox_max};
                if (!AabbAabbIntersect(obb_aabb, shape_aabb))
                    continue;
                printf("[DOOR DEBUG]   DOOR shape[%zu] bbox=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f) tri_count=%zu\n",
                       i,
                       cs.bbox_min.x, cs.bbox_min.y, cs.bbox_min.z,
                       cs.bbox_max.x, cs.bbox_max.y, cs.bbox_max.z,
                       cs.triangles.size());
            }
        }

        // Quebrar cobweb quando o player cai/pisa sobre ela
        if (g_PlayerVerticalVelocity < 5.0f)
        {
            CollisionAABB obb_aabb = ComputeObbAabb(test_obb);
            bool broke_any = false;
            for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
            {
                const auto &cs = g_ScenarioCollisionShapes[i];
                if (cs.type != CollisionShapeType::COBWEB_FLOORHOLE)
                    continue;
                if (IsCobwebBroken(cs))
                    continue;
                CollisionAABB shape_aabb = {cs.bbox_min, cs.bbox_max};
                if (!AabbAabbIntersect(obb_aabb, shape_aabb))
                    continue;

                glm::vec4 center = (cs.bbox_min + cs.bbox_max) * 0.5f;
                for (auto &cw : g_Cobwebs)
                {
                    float dx = cw.bbox_center.x - center.x;
                    float dz = cw.bbox_center.z - center.z;
                    if (dx * dx + dz * dz < 1.0f)
                    {
                        cw.broken = true;
                        broke_any = true;
                        printf("Cobweb quebrada em (%.1f, %.1f, %.1f)!\n",
                               cw.bbox_center.x, cw.bbox_center.y, cw.bbox_center.z);
                        break;
                    }
                }
            }
            if (broke_any)
                yFree = IsPositionFree(test_pos_y, g_PlayerCubeHalfExtents, g_PlayerYaw);
        }
    }

    if (yFree)
    {
        updated_position.y = test_pos_y.y;
        if (!g_IsClimbingALadder && !g_IsClimbingAVine)
            g_PlayerOnGround = false;
    }
    else
    {
        g_PlayerVerticalVelocity = 0.0f;
        if (!g_IsClimbingALadder && !g_IsClimbingAVine)
            g_PlayerOnGround = true;
    }

    // Ground snap: se não está no chão e não está pulando/escalando,
    // tenta puxar o player para baixo até encontrar o chão.
    // Isso corrige flutuação causada por step climbing e evita cair através do chão.
    if (!g_IsClimbingALadder && !g_IsClimbingAVine && !g_PlayerOnGround && g_PlayerVerticalVelocity <= 0.0f)
    {
        const float max_snap = 0.5f;
        const float snap_step = 0.02f;
        for (float snap = snap_step; snap <= max_snap + 0.001f; snap += snap_step)
        {
            glm::vec4 snap_pos = updated_position;
            snap_pos.y -= snap;
            if (!IsPositionFree(snap_pos, g_PlayerCubeHalfExtents, g_PlayerYaw))
            {
                updated_position.y = snap_pos.y + snap_step;
                g_PlayerVerticalVelocity = 0.0f;
                g_PlayerOnGround = true;
                break;
            }
        }
    }

    g_PlayerCubePosition = updated_position;

    glm::vec4 detection_extents = g_PlayerCubeHalfExtents * 1.1f;
    CollisionOBB detection_obb = {g_PlayerCubePosition, detection_extents, g_PlayerYaw};
    g_CollidedWithAVine = IsCollidingWithTypeObb(detection_obb, g_ScenarioCollisionShapes, CollisionShapeType::VINES);
    g_CollidedWithALadder = IsCollidingWithTypeObb(detection_obb, g_ScenarioCollisionShapes, CollisionShapeType::LADDER);

    // Detectar quebra de cobweb por queda
    if (!g_PlayerOnGround && g_PlayerVerticalVelocity < g_CobwebFallSpeedThreshold)
    {
        CollisionOBB cobweb_test = {g_PlayerCubePosition, g_PlayerCubeHalfExtents, g_PlayerYaw};
        const CollisionAABB cobweb_aabb = ComputeObbAabb(cobweb_test);

        for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
        {
            const CollisionShape &shape = g_ScenarioCollisionShapes[i];
            if (shape.type != CollisionShapeType::COBWEB_FLOORHOLE)
                continue;
            if (IsCobwebBroken(shape))
                continue;

            CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
            if (!AabbAabbIntersect(cobweb_aabb, shape_aabb))
                continue;

            glm::vec4 center = (shape.bbox_min + shape.bbox_max) * 0.5f;
            for (auto &cw : g_Cobwebs)
            {
                float dx = cw.bbox_center.x - center.x;
                float dz = cw.bbox_center.z - center.z;
                if (dx * dx + dz * dz < 1.0f)
                {
                    cw.broken = true;
                    printf("Cobweb quebrada em (%.1f, %.1f, %.1f)!\n",
                           cw.bbox_center.x, cw.bbox_center.y, cw.bbox_center.z);
                    break;
                }
            }
        }
    }

    CollisionOBB real_obb = {g_PlayerCubePosition, g_PlayerCubeHalfExtents, g_PlayerYaw};
    CollisionShapeType real_col = CollidesWithScenarioObb(real_obb, g_ScenarioCollisionShapes);
    g_PlayerCubeColliding = (real_col == CollisionShapeType::SOLID || real_col == CollisionShapeType::DOOR || real_col == CollisionShapeType::WATER);

    if (g_IsClimbingAVine || g_IsClimbingALadder)
    {
        static int colDebugCounter = 0;
        colDebugCounter++;
        if (colDebugCounter % 60 == 0)
        {
            printf("[CLIMB COL] vine=%d ladder=%d real=%d pos=(%.1f,%.1f,%.1f) yaw=%.1f\n",
                   g_CollidedWithAVine, g_CollidedWithALadder, (int)real_col,
                   g_PlayerCubePosition.x, g_PlayerCubePosition.y, g_PlayerCubePosition.z,
                   g_PlayerYaw);
        }
    }

    // Auto-stop: quando não colide mais com vine/ladder, para após grace period
    // independente do input (evita voar segurando W)
    static float vineGraceTimer = 0.0f;
    static float ladderGraceTimer = 0.0f;
    const float grace_period = 0.3f;

    if (g_IsClimbingAVine)
    {
        if (!g_CollidedWithAVine)
        {
            vineGraceTimer += delta_time;
            if (vineGraceTimer > grace_period)
            {
                g_IsClimbingAVine = false;
                vineGraceTimer = 0.0f;
                CollisionOBB ground_test = {g_PlayerCubePosition, g_PlayerCubeHalfExtents, g_PlayerYaw};
                CollisionShapeType ground_col = CollidesWithScenarioObb(ground_test, g_ScenarioCollisionShapes);
                g_PlayerOnGround = (ground_col == CollisionShapeType::SOLID);
                g_PlayerVerticalVelocity = 0.0f;
                g_SpacePressed = false;
                g_AttackPressed = false;
            }
        }
        else
        {
            vineGraceTimer = 0.0f;
        }
    }
    if (g_IsClimbingALadder)
    {
        if (!g_CollidedWithALadder)
        {
            ladderGraceTimer += delta_time;
            if (ladderGraceTimer > grace_period)
            {
                g_IsClimbingALadder = false;
                ladderGraceTimer = 0.0f;
                CollisionOBB ground_test = {g_PlayerCubePosition, g_PlayerCubeHalfExtents, g_PlayerYaw};
                CollisionShapeType ground_col = CollidesWithScenarioObb(ground_test, g_ScenarioCollisionShapes);
                g_PlayerOnGround = (ground_col == CollisionShapeType::SOLID);
                g_PlayerVerticalVelocity = 0.0f;
                g_SpacePressed = false;
                g_AttackPressed = false;
            }
        }
        else
        {
            ladderGraceTimer = 0.0f;
        }
    }

    glfwSetWindowTitle(
        window,
        g_PlayerCubeColliding ? "INF01047 - Colisao: DETECTADA" : "INF01047 - Colisao: livre");

    return move_input;
}

void SnapPlayerToNearestVine()
{
    glm::vec4 detection_extents = g_PlayerCubeHalfExtents * 2.0f;
    CollisionOBB detection_obb = {g_PlayerCubePosition, detection_extents, g_PlayerYaw};

    float best_dist = 1e30f;
    glm::vec4 best_point(0.0f);
    glm::vec4 best_normal(0.0f);

    for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
    {
        const CollisionShape &shape = g_ScenarioCollisionShapes[i];
        if (shape.type != CollisionShapeType::VINES && shape.type != CollisionShapeType::LADDER)
            continue;

        const CollisionAABB obb_aabb = ComputeObbAabb(detection_obb);
        CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
        if (!AabbAabbIntersect(obb_aabb, shape_aabb))
            continue;

        for (size_t t = 0; t < shape.triangles.size(); ++t)
        {
            if (!TriangleIntersectsObb(shape.triangles[t], detection_obb))
                continue;

            const Triangle &tri = shape.triangles[t];
            glm::vec4 e1 = tri.v2 - tri.v1;
            glm::vec4 e2 = tri.v3 - tri.v1;
            glm::vec4 n = crossproduct(e1, e2);
            float nlen = std::sqrt(dotproduct(n, n));
            if (nlen < 1e-6f)
                continue;
            n = n / nlen;
            n.w = 0.0f;

            // Closest point on triangle to player (in XZ plane)
            glm::vec4 to_player = g_PlayerCubePosition - tri.v1;
            to_player.y = 0.0f;
            float d1 = dotproduct(e1, to_player);
            float d2 = dotproduct(e2, to_player);
            float dot00 = dotproduct(e1, e1);
            float dot11 = dotproduct(e2, e2);
            float dot01 = dotproduct(e1, e2);
            float denom = dot00 * dot11 - dot01 * dot01;
            if (std::abs(denom) < 1e-6f)
                continue;
            float u = (d1 * dot11 - d2 * dot01) / denom;
            float v = (d2 * dot00 - d1 * dot01) / denom;
            u = std::max(0.0f, std::min(1.0f, u));
            v = std::max(0.0f, std::min(1.0f - u, v));
            glm::vec4 closest = tri.v1 + u * e1 + v * e2;

            float dx = closest.x - g_PlayerCubePosition.x;
            float dz = closest.z - g_PlayerCubePosition.z;
            float dist = dx * dx + dz * dz;
            if (dist < best_dist)
            {
                best_dist = dist;
                best_point = closest;
                best_normal = n;
            }
        }
    }

    if (best_dist < 1e29f)
    {
        // Snap XZ to closest point, offset slightly outward along normal
        g_PlayerCubePosition.x = best_point.x + best_normal.x * 0.1f;
        g_PlayerCubePosition.z = best_point.z + best_normal.z * 0.1f;
        printf("[SNAP VINE] Snapped to vine surface at (%.1f, %.1f, %.1f) normal=(%.1f,%.1f,%.1f)\n",
               g_PlayerCubePosition.x, g_PlayerCubePosition.y, g_PlayerCubePosition.z,
               best_normal.x, best_normal.y, best_normal.z);
    }
}
