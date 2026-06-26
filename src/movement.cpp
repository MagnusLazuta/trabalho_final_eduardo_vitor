#include "movement.h"

#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>
#include <cmath>

#include "collision.h"
#include "globals.h"
#include "types.h"

bool QueryBlockingEnemyCollision(const glm::vec4 &center, const glm::vec4 &half_extents);

static bool IsCollidingWithGhostLadder(const CollisionOBB &obb)
{
    for (const auto &gl : g_GhostLadders)
    {
        if (gl.state != GhostLadderState::GROUNDED)
            continue;

        CollisionAABB obb_aabb = ComputeObbAabb(obb);
        glm::vec4 adj_min = gl.bbox_min + glm::vec4(0.0f, gl.current_y_offset, 0.0f, 0.0f);
        glm::vec4 adj_max = gl.bbox_max + glm::vec4(0.0f, gl.current_y_offset, 0.0f, 0.0f);

        if (obb_aabb.max.x < adj_min.x || obb_aabb.min.x > adj_max.x ||
            obb_aabb.max.y < adj_min.y || obb_aabb.min.y > adj_max.y ||
            obb_aabb.max.z < adj_min.z || obb_aabb.min.z > adj_max.z)
            continue;

        for (const auto &tri : gl.original_triangles)
        {
            Triangle offset_tri = tri;
            offset_tri.v1.y += gl.current_y_offset;
            offset_tri.v2.y += gl.current_y_offset;
            offset_tri.v3.y += gl.current_y_offset;
            if (TriangleIntersectsObb(offset_tri, obb))
                return true;
        }
    }
    return false;
}

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
            shape.type != CollisionShapeType::GROUND &&
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

    if (g_IsClimbingALadder || g_IsClimbingAVine || g_IsClimbingAGhostLadder)
    {
        g_PlayerVerticalVelocity = 0.0f;
        float climb_speed = (g_IsClimbingALadder || g_IsClimbingAGhostLadder) ? 2.0f : 1.5f;
        if (g_WPressed)
            vertical_move_amount += climb_speed * delta_time;
        if (g_SPressed)
            vertical_move_amount -= climb_speed * delta_time;

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
    if (g_IsClimbingALadder || g_IsClimbingAVine || g_IsClimbingAGhostLadder)
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

        printf("[COBWEB] !yFree: type=%d test_pos.y=%.2f vel=%.2f thresh=%.2f curPos.y=%.2f\n",
               (int)block_col, test_pos_y.y, g_PlayerVerticalVelocity,
               g_CobwebFallSpeedThreshold, g_PlayerCubePosition.y);

        // Quebrar cobweb quando o player cai/pisa sobre ela
        if (g_PlayerVerticalVelocity < g_CobwebFallSpeedThreshold)
        {
            printf("[COBWEB M1] !yFree, vel=%.2f, test_pos.y=%.2f, block_col=%d, n_cobwebs=%zu\n",
                   g_PlayerVerticalVelocity, test_pos_y.y, (int)block_col, g_Cobwebs.size());
            CollisionAABB obb_aabb = ComputeObbAabb(test_obb);
            // Expande Y do AABB para cobrir o volume varrido ate a posicao de teste
            float prev_y_m1 = g_PlayerCubePosition.y;
            float player_top_m1 = std::max(prev_y_m1 + g_PlayerCubeHalfExtents.y, test_pos_y.y + g_PlayerCubeHalfExtents.y);
            float player_bottom_m1 = std::min(prev_y_m1 - g_PlayerCubeHalfExtents.y, test_pos_y.y - g_PlayerCubeHalfExtents.y);
            obb_aabb.min.y = player_bottom_m1;
            obb_aabb.max.y = player_top_m1;
            printf("[COBWEB M1] sweep Y: prev=%.2f test=%.2f halfExt=%.2f -> range [%.2f, %.2f]\n",
                   prev_y_m1, test_pos_y.y, g_PlayerCubeHalfExtents.y, player_bottom_m1, player_top_m1);
            bool broke_any = false;
            for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
            {
                const auto &cs = g_ScenarioCollisionShapes[i];
                if (cs.type != CollisionShapeType::COBWEB_FLOORHOLE)
                    continue;
                if (IsCobwebBroken(cs))
                    continue;
                printf("[COBWEB M1] cobweb shape #%zu: bbox_min=(%.2f,%.2f,%.2f) bbox_max=(%.2f,%.2f,%.2f) triangles=%zu\n",
                       i, cs.bbox_min.x, cs.bbox_min.y, cs.bbox_min.z,
                       cs.bbox_max.x, cs.bbox_max.y, cs.bbox_max.z, cs.triangles.size());
                CollisionAABB shape_aabb = {cs.bbox_min, cs.bbox_max};
                if (!AabbAabbIntersect(obb_aabb, shape_aabb))
                {
                    printf("[COBWEB M1] AABB miss: obb=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)\n",
                           obb_aabb.min.x, obb_aabb.min.y, obb_aabb.min.z,
                           obb_aabb.max.x, obb_aabb.max.y, obb_aabb.max.z);
                    continue;
                }
                printf("[COBWEB M1] AABB HIT!\n");

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
        if (!g_IsClimbingALadder && !g_IsClimbingAVine && !g_IsClimbingAGhostLadder)
            g_PlayerOnGround = false;
    }
    else
    {
        g_PlayerVerticalVelocity = 0.0f;
        if (!g_IsClimbingALadder && !g_IsClimbingAVine && !g_IsClimbingAGhostLadder)
            g_PlayerOnGround = true;
    }

    // Ground snap: se não está no chão e não está pulando/escalando,
    // tenta puxar o player para baixo até encontrar o chão.
    // Isso corrige flutuação causada por step climbing e evita cair através do chão.
    if (!g_IsClimbingALadder && !g_IsClimbingAVine && !g_IsClimbingAGhostLadder && !g_PlayerOnGround && g_PlayerVerticalVelocity <= 0.0f)
    {
        const float max_snap = 0.5f;
        const float snap_step = 0.02f;
        for (float snap = snap_step; snap <= max_snap + 0.001f; snap += snap_step)
        {
            glm::vec4 snap_pos = updated_position;
            snap_pos.y -= snap;
            if (!IsPositionFree(snap_pos, g_PlayerCubeHalfExtents, g_PlayerYaw))
            {
                // Verifica quebra de cobweb durante o ground snap
                if (g_PlayerVerticalVelocity < g_CobwebFallSpeedThreshold)
                {
                    CollisionOBB snap_obb = {snap_pos, g_PlayerCubeHalfExtents, g_PlayerYaw};
                    CollisionAABB snap_aabb = ComputeObbAabb(snap_obb);
                    bool broke_cobweb = false;
                    for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
                    {
                        const auto &cs = g_ScenarioCollisionShapes[i];
                        if (cs.type != CollisionShapeType::COBWEB_FLOORHOLE)
                            continue;
                        if (IsCobwebBroken(cs))
                            continue;
                        CollisionAABB shape_aabb = {cs.bbox_min, cs.bbox_max};
                        if (!AabbAabbIntersect(snap_aabb, shape_aabb))
                            continue;
                        glm::vec4 center = (cs.bbox_min + cs.bbox_max) * 0.5f;
                        for (auto &cw : g_Cobwebs)
                        {
                            float dx = cw.bbox_center.x - center.x;
                            float dz = cw.bbox_center.z - center.z;
                            if (dx * dx + dz * dz < 1.0f)
                            {
                                cw.broken = true;
                                broke_cobweb = true;
                                printf("[COBWEB SNAP] Cobweb quebrada via ground snap! vel=%.2f\n", g_PlayerVerticalVelocity);
                                break;
                            }
                        }
                        if (broke_cobweb)
                            break;
                    }
                    if (broke_cobweb)
                        continue;
                }
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
    g_CollidedWithAGhostLadder = IsCollidingWithGhostLadder(detection_obb);

    // Detectar quebra de cobweb por queda
    if (!g_PlayerOnGround && g_PlayerVerticalVelocity < g_CobwebFallSpeedThreshold)
    {
        printf("[COBWEB M2] noGround, vel=%.2f, pos.y=%.2f, threshold=%.2f\n",
               g_PlayerVerticalVelocity, g_PlayerCubePosition.y, g_CobwebFallSpeedThreshold);
        CollisionOBB cobweb_test = {g_PlayerCubePosition, g_PlayerCubeHalfExtents, g_PlayerYaw};
        CollisionAABB cobweb_aabb = ComputeObbAabb(cobweb_test);

        // Expande Y do AABB para cobrir o volume varrido durante a queda deste frame
        float prev_y = g_PlayerCubePosition.y - vertical_move_amount;
        float player_top = std::max(prev_y + g_PlayerCubeHalfExtents.y, g_PlayerCubePosition.y + g_PlayerCubeHalfExtents.y);
        float player_bottom = std::min(prev_y - g_PlayerCubeHalfExtents.y, g_PlayerCubePosition.y - g_PlayerCubeHalfExtents.y);
        cobweb_aabb.min.y = player_bottom;
        cobweb_aabb.max.y = player_top;
        printf("[COBWEB M2] sweep Y: prev=%.2f cur=%.2f halfExt=%.2f -> range [%.2f, %.2f]\n",
               prev_y, g_PlayerCubePosition.y, g_PlayerCubeHalfExtents.y, player_bottom, player_top);

        for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
        {
            const CollisionShape &shape = g_ScenarioCollisionShapes[i];
            if (shape.type != CollisionShapeType::COBWEB_FLOORHOLE)
                continue;
            if (IsCobwebBroken(shape))
                continue;

            CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
            if (!AabbAabbIntersect(cobweb_aabb, shape_aabb))
            {
                printf("[COBWEB M2] AABB miss shape #%zu\n", i);
                continue;
            }
            printf("[COBWEB M2] AABB HIT shape #%zu!\n", i);

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

    // Status periodico das cobwebs a cada 120 frames
    {
        static int cobweb_count_frame = 0;
        if (++cobweb_count_frame >= 120)
        {
            cobweb_count_frame = 0;
            int total = 0, unbroken = 0, in_scenario = 0;
            for (const auto &cw : g_Cobwebs)
            {
                total++;
                if (!cw.broken) unbroken++;
            }
            for (const auto &cs : g_ScenarioCollisionShapes)
            {
                if (cs.type == CollisionShapeType::COBWEB_FLOORHOLE)
                {
                    in_scenario++;
                    printf("[COBWEB STATUS] shape #%zu: bbox=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f) tri=%zu type=%d\n",
                           (size_t)(&cs - &g_ScenarioCollisionShapes[0]),
                           cs.bbox_min.x, cs.bbox_min.y, cs.bbox_min.z,
                           cs.bbox_max.x, cs.bbox_max.y, cs.bbox_max.z,
                           cs.triangles.size(), (int)cs.type);
                }
            }
            printf("[COBWEB STATUS] total=%d unbroken=%d inScenarioShapes=%d onGround=%d vel=%.2f pos=(%.1f,%.1f,%.1f) halfExt=(%.2f,%.2f,%.2f)\n",
                   total, unbroken, in_scenario, (int)g_PlayerOnGround,
                   g_PlayerVerticalVelocity,
                   g_PlayerCubePosition.x, g_PlayerCubePosition.y, g_PlayerCubePosition.z,
                   g_PlayerCubeHalfExtents.x, g_PlayerCubeHalfExtents.y, g_PlayerCubeHalfExtents.z);
        }
    }

    CollisionOBB real_obb = {g_PlayerCubePosition, g_PlayerCubeHalfExtents, g_PlayerYaw};
    CollisionShapeType real_col = CollidesWithScenarioObb(real_obb, g_ScenarioCollisionShapes);
    g_PlayerCubeColliding = (real_col == CollisionShapeType::SOLID || real_col == CollisionShapeType::GROUND || real_col == CollisionShapeType::DOOR || real_col == CollisionShapeType::WATER);

    // Auto-stop: quando não colide mais com vine/ladder, para após grace period
    // independente do input (evita voar segurando W)
    static float vineGraceTimer = 0.0f;
    static float ladderGraceTimer = 0.0f;
    static float ghostLadderGraceTimer = 0.0f;
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
                g_PlayerOnGround = (ground_col == CollisionShapeType::SOLID || ground_col == CollisionShapeType::GROUND);;
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
                g_PlayerOnGround = (ground_col == CollisionShapeType::SOLID || ground_col == CollisionShapeType::GROUND);;
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
    if (g_IsClimbingAGhostLadder)
    {
        if (!g_CollidedWithAGhostLadder)
        {
            ghostLadderGraceTimer += delta_time;
            if (ghostLadderGraceTimer > grace_period)
            {
                g_IsClimbingAGhostLadder = false;
                ghostLadderGraceTimer = 0.0f;
                CollisionOBB ground_test = {g_PlayerCubePosition, g_PlayerCubeHalfExtents, g_PlayerYaw};
                CollisionShapeType ground_col = CollidesWithScenarioObb(ground_test, g_ScenarioCollisionShapes);
                g_PlayerOnGround = (ground_col == CollisionShapeType::SOLID || ground_col == CollisionShapeType::GROUND);;
                g_PlayerVerticalVelocity = 0.0f;
                g_SpacePressed = false;
                g_AttackPressed = false;
            }
        }
        else
        {
            ghostLadderGraceTimer = 0.0f;
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

    // Snap to grounded ghost ladders
    for (const auto &gl : g_GhostLadders)
    {
        if (gl.state != GhostLadderState::GROUNDED)
            continue;

        glm::vec4 adj_min = gl.bbox_min + glm::vec4(0.0f, gl.current_y_offset, 0.0f, 0.0f);
        glm::vec4 adj_max = gl.bbox_max + glm::vec4(0.0f, gl.current_y_offset, 0.0f, 0.0f);
        CollisionAABB gl_aabb = {adj_min, adj_max};
        const CollisionAABB obb_aabb = ComputeObbAabb(detection_obb);
        if (!AabbAabbIntersect(obb_aabb, gl_aabb))
            continue;

        for (const auto &tri : gl.original_triangles)
        {
            Triangle offset_tri = tri;
            offset_tri.v1.y += gl.current_y_offset;
            offset_tri.v2.y += gl.current_y_offset;
            offset_tri.v3.y += gl.current_y_offset;
            if (!TriangleIntersectsObb(offset_tri, detection_obb))
                continue;

            glm::vec4 e1 = offset_tri.v2 - offset_tri.v1;
            glm::vec4 e2 = offset_tri.v3 - offset_tri.v1;
            glm::vec4 n = crossproduct(e1, e2);
            float nlen = std::sqrt(dotproduct(n, n));
            if (nlen < 1e-6f)
                continue;
            n = n / nlen;
            n.w = 0.0f;

            glm::vec4 to_player = g_PlayerCubePosition - offset_tri.v1;
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
            glm::vec4 closest = offset_tri.v1 + u * e1 + v * e2;

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
    }
}
