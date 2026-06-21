#include "enemies.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/type_ptr.hpp>

#include "matrices.h"
#include "movement.h"

std::vector<Enemy> g_Enemies;
std::vector<Enemy> g_PendingEnemySpawns;
std::vector<EnemyProjectile> g_EnemyProjectiles;
float g_PlayerHitLogCooldown = 0.0f;

static const float PI = 3.141592f;
static const float ENEMY_COLLISION_MARGIN = 0.08f;
static const float DEKU_BABA_BITE_WINDUP = 0.35f;
static const float DEKU_BABA_BITE_ACTIVE_TIME = 0.22f;
static const float DEKU_BABA_BITE_RECOVERY = 0.55f;
static const float DEKU_SCRUB_HIDDEN_HEIGHT = -0.45f;
static const float DEKU_SCRUB_STUN_DURATION = 3.0f;
static const float BIG_SKULLTULA_ATTACK_DURATION = 0.85f;
static const float BIG_SKULLTULA_VULNERABLE_DURATION = 1.2f;
static const float GOHMA_LARVA_JUMP_DURATION = 0.7f;
static const float QUEEN_GOHMA_STUN_DURATION = 4.0f;

static const RenderModelInfo g_EmptyRenderModelInfo = {false, {}, glm::vec4(0.0f), 1.0f};

static float Clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

static float LengthXZ(const glm::vec4 &v)
{
    return std::sqrt(v.x * v.x + v.z * v.z);
}

static float DistanceXZ(const glm::vec4 &a, const glm::vec4 &b)
{
    return LengthXZ(a - b);
}

static glm::vec4 NormalizeXZ(const glm::vec4 &v)
{
    const float len = LengthXZ(v);
    if (len <= 1e-5f)
        return glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);

    return glm::vec4(v.x / len, 0.0f, v.z / len, 0.0f);
}

static float ComputeYawToTarget(const glm::vec4 &from, const glm::vec4 &to)
{
    const glm::vec4 delta = to - from;
    return std::atan2(delta.x, delta.z);
}

static glm::vec4 ForwardFromYaw(float yaw)
{
    return glm::vec4(std::sin(yaw), 0.0f, std::cos(yaw), 0.0f);
}

static void LogPlayerHitByEnemy(const char *enemy_name)
{
    if (g_PlayerHitLogCooldown > 0.0f)
        return;

    printf("[TEST] Jogador atingido por %s\n", enemy_name);
    g_PlayerHitLogCooldown = 0.75f;
}

static Enemy MakeEnemy(
    EnemyType type,
    EnemyState state,
    const glm::vec4 &position,
    const glm::vec4 &scale,
    const glm::vec4 &collision_half_extents,
    int health,
    float detection_radius,
    float attack_radius,
    float attack_cooldown,
    const std::string &object_name)
{
    Enemy enemy;
    enemy.type = type;
    enemy.state = state;
    enemy.position = position;
    enemy.spawn_position = position;
    enemy.velocity = glm::vec4(0.0f);
    enemy.scale = scale;
    enemy.collision_half_extents = collision_half_extents;
    enemy.yaw = 0.0f;
    enemy.pitch = 0.0f;
    enemy.health = health;
    enemy.detection_radius = detection_radius;
    enemy.attack_radius = attack_radius;
    enemy.attack_cooldown = attack_cooldown;
    enemy.attack_cooldown_timer = 0.0f;
    enemy.state_timer = 0.0f;
    enemy.animation_timer = 0.0f;
    enemy.timer_a = 0.0f;
    enemy.timer_b = 0.0f;
    enemy.active = true;
    enemy.dead = false;
    enemy.vulnerable = false;
    enemy.visible = true;
    enemy.blocks_movement = false;
    enemy.has_spawned_helpers = false;
    enemy.debug_color = glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);
    enemy.object_name = object_name;
    return enemy;
}

static void QueueEnemySpawn(const Enemy &enemy)
{
    g_PendingEnemySpawns.push_back(enemy);
}

static const RenderModelInfo &GetEnemyRenderInfo(const Enemy &enemy, const EnemyRenderResources &render_resources)
{
    switch (enemy.type)
    {
    case EnemyType::DEKU_BABA:
    case EnemyType::WITHERED_DEKU_BABA:
        return render_resources.deku_baba_render_info ? *render_resources.deku_baba_render_info : g_EmptyRenderModelInfo;
    case EnemyType::DEKU_SCRUB:
        return render_resources.deku_scrub_render_info ? *render_resources.deku_scrub_render_info : g_EmptyRenderModelInfo;
    case EnemyType::QUEEN_GOHMA:
        return render_resources.queen_gohma_render_info ? *render_resources.queen_gohma_render_info : g_EmptyRenderModelInfo;
    case EnemyType::SKULLWALLTULA:
    case EnemyType::BIG_SKULLTULA:
    case EnemyType::GOHMA_LARVA:
    default:
        return render_resources.sphere_render_info ? *render_resources.sphere_render_info : g_EmptyRenderModelInfo;
    }
}

static bool IsEnemyUsingPlaceholder(const Enemy &enemy, const EnemyRenderResources &render_resources)
{
    return !GetEnemyRenderInfo(enemy, render_resources).available ||
           enemy.type == EnemyType::SKULLWALLTULA ||
           enemy.type == EnemyType::BIG_SKULLTULA ||
           enemy.type == EnemyType::GOHMA_LARVA;
}

static glm::mat4 BuildEnemyModelMatrix(const Enemy &enemy, const EnemyRenderResources &render_resources)
{
    const RenderModelInfo &render_info = GetEnemyRenderInfo(enemy, render_resources);
    glm::mat4 model =
        Matrix_Translate(enemy.position.x, enemy.position.y, enemy.position.z) *
        Matrix_Rotate_Y(enemy.yaw);

    if (std::fabs(enemy.pitch) > 1e-4f)
        model = model * Matrix_Rotate_X(enemy.pitch);

    model = model *
            Matrix_Scale(
                enemy.scale.x * render_info.base_scale,
                enemy.scale.y * render_info.base_scale,
                enemy.scale.z * render_info.base_scale);

    if (render_info.available)
        model = model * Matrix_Translate(-render_info.center.x, -render_info.center.y, -render_info.center.z);

    return model;
}

static bool EnemyAabbIntersectsPointBox(const Enemy &enemy, const glm::vec4 &center, const glm::vec4 &half_extents)
{
    glm::vec4 expanded_half_extents = enemy.collision_half_extents;

    if (enemy.type == EnemyType::DEKU_SCRUB)
    {
        expanded_half_extents.x += 0.35f;
        expanded_half_extents.y += 0.35f;
        expanded_half_extents.z += 0.35f;
    }
    else if (enemy.type == EnemyType::QUEEN_GOHMA)
    {
        expanded_half_extents.x += 0.12f;
        expanded_half_extents.y += 0.12f;
        expanded_half_extents.z += 0.12f;
    }

    CollisionAABB enemy_box = {
        enemy.position - expanded_half_extents,
        enemy.position + expanded_half_extents};
    CollisionAABB other_box = {
        center - half_extents,
        center + half_extents};

    return AabbAabbIntersect(enemy_box, other_box);
}

static bool BoxesIntersect(
    const glm::vec4 &center_a,
    const glm::vec4 &half_extents_a,
    const glm::vec4 &center_b,
    const glm::vec4 &half_extents_b)
{
    CollisionAABB box_a = {center_a - half_extents_a, center_a + half_extents_a};
    CollisionAABB box_b = {center_b - half_extents_b, center_b + half_extents_b};
    return AabbAabbIntersect(box_a, box_b);
}

static void MoveEnemyWithScenarioCollision(
    Enemy &enemy,
    const glm::vec4 &movement,
    const std::vector<CollisionShape> &scenario_collision_shapes)
{
    glm::vec4 updated_position = enemy.position;
    glm::vec4 test_half_extents = enemy.collision_half_extents;
    test_half_extents.y = std::max(0.08f, test_half_extents.y - ENEMY_COLLISION_MARGIN);

    glm::vec4 test_position_x = updated_position;
    test_position_x.x += movement.x;
    CollisionShapeType collision_x = CollidesWithScenario(test_position_x, scenario_collision_shapes, test_half_extents);
    if (collision_x != CollisionShapeType::SOLID && collision_x != CollisionShapeType::DOOR)
        updated_position.x = test_position_x.x;

    glm::vec4 test_position_z = updated_position;
    test_position_z.z += movement.z;
    CollisionShapeType collision_z = CollidesWithScenario(test_position_z, scenario_collision_shapes, test_half_extents);
    if (collision_z != CollisionShapeType::SOLID && collision_z != CollisionShapeType::DOOR)
        updated_position.z = test_position_z.z;

    enemy.position = updated_position;
}

static void KillEnemy(Enemy &enemy)
{
    enemy.dead = true;
    enemy.active = false;
    enemy.visible = false;
    enemy.vulnerable = false;
    enemy.state = (enemy.type == EnemyType::QUEEN_GOHMA) ? EnemyState::BossDead : EnemyState::Dead;

    // TODO: Integrar drops (Deku Stick, Deku Nut e recompensas do chefe).
}

static void ApplyDamageToEnemy(Enemy &enemy, int damage)
{
    if (enemy.dead || !enemy.active)
        return;

    if (!enemy.vulnerable)
    {
        if (enemy.type == EnemyType::DEKU_SCRUB)
        {
            enemy.state = EnemyState::Stunned;
            enemy.state_timer = 0.0f;
            enemy.vulnerable = true;
            // TODO: Trocar este stun temporario pela reflexao correta do escudo.
        }
        else if (enemy.type == EnemyType::QUEEN_GOHMA)
        {
            enemy.state = EnemyState::BossStunned;
            enemy.state_timer = 0.0f;
            enemy.vulnerable = true;
            // TODO: Restringir este stun ao olho/condicoes corretas quando o combate final estiver completo.
        }
        else
        {
            return;
        }
    }

    enemy.health -= damage;
    if (enemy.health <= 0)
    {
        KillEnemy(enemy);
        return;
    }

    if (enemy.type == EnemyType::DEKU_SCRUB)
    {
        enemy.state = EnemyState::Stunned;
        enemy.state_timer = 0.0f;
    }
    else if (enemy.type == EnemyType::QUEEN_GOHMA)
    {
        enemy.state = EnemyState::BossStunned;
        enemy.state_timer = 0.0f;
    }
}

static void SpawnEnemyProjectile(
    EnemyProjectileType type,
    const glm::vec4 &position,
    const glm::vec4 &velocity,
    float max_lifetime_seconds,
    float scale)
{
    EnemyProjectile projectile;
    projectile.type = type;
    projectile.position = position;
    projectile.velocity = velocity;
    projectile.scale = glm::vec4(scale, scale, scale, 0.0f);
    projectile.lifetime_seconds = 0.0f;
    projectile.max_lifetime_seconds = max_lifetime_seconds;
    projectile.active = true;
    g_EnemyProjectiles.push_back(projectile);
}

static void SpawnDekuScrubProjectile(const Enemy &enemy, const glm::vec4 &direction_to_player)
{
    const glm::vec4 velocity = NormalizeXZ(direction_to_player) * 6.5f;
    SpawnEnemyProjectile(
        EnemyProjectileType::DEKU_NUT,
        enemy.position + glm::vec4(0.0f, 0.45f, 0.0f, 0.0f),
        velocity,
        3.2f,
        0.42f);
}

static void SpawnGohmaLarvaFromEgg(const glm::vec4 &position)
{
    Enemy larva = MakeEnemy(
        EnemyType::GOHMA_LARVA,
        EnemyState::Idle,
        position,
        glm::vec4(1.50f, 1.10f, 1.50f, 0.0f),
        glm::vec4(0.64f, 0.40f, 0.64f, 0.0f),
        2,
        5.5f,
        1.0f,
        1.6f,
        "gohma_larva_placeholder");
    larva.visible = true;
    larva.vulnerable = true;
    larva.debug_color = glm::vec4(0.25f, 0.82f, 0.36f, 1.0f);
    QueueEnemySpawn(larva);
}

static void SpawnQueenGohmaEggWave(const Enemy &enemy)
{
    const glm::vec4 offsets[3] = {
        glm::vec4(1.1f, 0.0f, 1.0f, 0.0f),
        glm::vec4(-1.2f, 0.0f, 0.5f, 0.0f),
        glm::vec4(0.2f, 0.0f, -1.3f, 0.0f)};

    for (int i = 0; i < 3; ++i)
        SpawnGohmaLarvaFromEgg(enemy.position + offsets[i]);
}

static void UpdateDekuBaba(Enemy &enemy, float delta_time, const EnemyUpdateContext &context)
{
    const float distance_to_player = DistanceXZ(enemy.position, context.player_position);
    enemy.vulnerable = false;
    enemy.pitch = 0.0f;

    if (enemy.attack_cooldown_timer > 0.0f)
        enemy.attack_cooldown_timer = std::max(0.0f, enemy.attack_cooldown_timer - delta_time);

    if (distance_to_player <= enemy.detection_radius)
        enemy.yaw = ComputeYawToTarget(enemy.position, context.player_position);

    switch (enemy.state)
    {
    case EnemyState::Idle:
        if (distance_to_player <= enemy.detection_radius)
            enemy.pitch = 0.18f * std::sin(enemy.animation_timer * 3.5f);

        if (distance_to_player <= enemy.attack_radius && enemy.attack_cooldown_timer <= 0.0f)
        {
            enemy.state = EnemyState::Attacking;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Attacking:
        if (enemy.state_timer < DEKU_BABA_BITE_WINDUP)
        {
            enemy.pitch = -0.45f * Clamp01(enemy.state_timer / DEKU_BABA_BITE_WINDUP);
        }
        else if (enemy.state_timer < DEKU_BABA_BITE_WINDUP + DEKU_BABA_BITE_ACTIVE_TIME)
        {
            enemy.pitch = 0.70f;
            if (DistanceXZ(enemy.position, context.player_position) <= enemy.attack_radius + 0.25f)
                LogPlayerHitByEnemy("Deku Baba");
            // TODO: Aplicar dano/knockback ao jogador durante a janela ativa da mordida.
        }
        else if (enemy.state_timer < DEKU_BABA_BITE_WINDUP + DEKU_BABA_BITE_ACTIVE_TIME + DEKU_BABA_BITE_RECOVERY)
        {
            enemy.pitch = 0.25f;
            enemy.vulnerable = true;
        }
        else
        {
            enemy.attack_cooldown_timer = enemy.attack_cooldown;
            enemy.state = EnemyState::Idle;
            enemy.state_timer = 0.0f;
        }
        break;

    default:
        enemy.state = EnemyState::Idle;
        enemy.state_timer = 0.0f;
        break;
    }

    if (enemy.state != EnemyState::Attacking)
    {
        enemy.vulnerable = true;
        // TODO: Integrar recebimento de dano com a hitbox de ataque corpo-a-corpo do jogador.
    }
}

static void UpdateWitheredDekuBaba(Enemy &enemy, float delta_time)
{
    (void)delta_time;
    enemy.state = EnemyState::Idle;
    enemy.yaw = enemy.timer_b;
    enemy.pitch = 0.08f * std::sin(enemy.animation_timer * 2.2f);
    enemy.vulnerable = true;
    // TODO: Fazer morrer com um golpe quando o sistema de ataque do jogador existir.
}

static void UpdateDekuScrub(Enemy &enemy, float delta_time, const EnemyUpdateContext &context)
{
    const glm::vec4 delta_to_player = context.player_position - enemy.position;
    const float distance_to_player = DistanceXZ(enemy.position, context.player_position);

    enemy.vulnerable = (enemy.state == EnemyState::Stunned);
    enemy.yaw = ComputeYawToTarget(enemy.position, context.player_position);

    if (enemy.attack_cooldown_timer > 0.0f)
        enemy.attack_cooldown_timer = std::max(0.0f, enemy.attack_cooldown_timer - delta_time);

    switch (enemy.state)
    {
    case EnemyState::Hidden:
        enemy.visible = true;
        enemy.position.y = enemy.spawn_position.y + DEKU_SCRUB_HIDDEN_HEIGHT;
        if (distance_to_player <= enemy.detection_radius)
        {
            enemy.state = EnemyState::Aiming;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Aiming:
        enemy.position.y = enemy.spawn_position.y;
        if (distance_to_player > enemy.detection_radius * 1.4f)
        {
            enemy.state = EnemyState::Hidden;
            enemy.state_timer = 0.0f;
            break;
        }

        if (enemy.attack_cooldown_timer <= 0.0f && enemy.state_timer >= 0.8f)
        {
            enemy.state = EnemyState::Shooting;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Shooting:
        enemy.position.y = enemy.spawn_position.y;
        if (!enemy.has_spawned_helpers && enemy.state_timer >= 0.15f)
        {
            SpawnDekuScrubProjectile(enemy, delta_to_player);
            enemy.has_spawned_helpers = true;
        }

        if (enemy.state_timer >= 0.45f)
        {
            enemy.has_spawned_helpers = false;
            enemy.attack_cooldown_timer = enemy.attack_cooldown;
            enemy.state = EnemyState::Aiming;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Stunned:
        enemy.position.y = enemy.spawn_position.y;
        enemy.vulnerable = true;
        if (enemy.state_timer >= DEKU_SCRUB_STUN_DURATION)
        {
            enemy.state = EnemyState::Hidden;
            enemy.state_timer = 0.0f;
        }
        break;

    default:
        enemy.state = EnemyState::Hidden;
        enemy.state_timer = 0.0f;
        break;
    }

    // TODO: Integrar reflexão do escudo para colocar o Deku Scrub em Stunned.
}

static void UpdateSkullwalltula(Enemy &enemy, float delta_time, const EnemyUpdateContext &context)
{
    (void)delta_time;
    enemy.position.x = enemy.spawn_position.x + 0.12f * std::sin(enemy.animation_timer * 0.7f);
    enemy.position.y = enemy.spawn_position.y;
    enemy.yaw = ComputeYawToTarget(enemy.position, context.player_position);
    enemy.vulnerable = true;

    if (DistanceXZ(enemy.position, context.player_position) <= enemy.attack_radius)
    {
        LogPlayerHitByEnemy("Skullwalltula");
        // TODO: Aplicar dano/knockback ao jogador ao aproximar de Skullwalltula.
    }
}

static void UpdateBigSkulltula(Enemy &enemy, float delta_time, const EnemyUpdateContext &context)
{
    const glm::vec4 to_player = NormalizeXZ(context.player_position - enemy.position);
    const glm::vec4 enemy_forward = ForwardFromYaw(enemy.yaw);
    const float front_dot = dotproduct(enemy_forward, to_player);

    enemy.yaw = WrapAnglePi(enemy.yaw + 0.55f * delta_time);
    enemy.vulnerable = false;

    switch (enemy.state)
    {
    case EnemyState::Idle:
    case EnemyState::Turning:
        enemy.state = EnemyState::Turning;
        if (front_dot < -0.25f)
        {
            enemy.state = EnemyState::Vulnerable;
            enemy.state_timer = 0.0f;
        }
        else if (DistanceXZ(enemy.position, context.player_position) <= enemy.attack_radius &&
                 enemy.attack_cooldown_timer <= 0.0f)
        {
            enemy.state = EnemyState::Attacking;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Vulnerable:
        enemy.vulnerable = true;
        if (front_dot >= -0.05f || enemy.state_timer >= BIG_SKULLTULA_VULNERABLE_DURATION)
        {
            enemy.state = EnemyState::Turning;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Attacking:
    {
        const glm::vec4 dash = ForwardFromYaw(enemy.yaw) * (1.8f * delta_time);
        MoveEnemyWithScenarioCollision(enemy, dash, *context.scenario_collision_shapes);

        if (enemy.state_timer >= BIG_SKULLTULA_ATTACK_DURATION)
        {
            enemy.attack_cooldown_timer = enemy.attack_cooldown;
            enemy.state = EnemyState::Vulnerable;
            enemy.state_timer = 0.0f;
        }

        if (DistanceXZ(enemy.position, context.player_position) <= enemy.attack_radius + 0.45f)
            LogPlayerHitByEnemy("Big Skulltula");
        // TODO: Aplicar dano no contato da investida/giro quando houver vida do jogador.
        break;
    }

    default:
        enemy.state = EnemyState::Turning;
        enemy.state_timer = 0.0f;
        break;
    }

    if (enemy.attack_cooldown_timer > 0.0f)
        enemy.attack_cooldown_timer = std::max(0.0f, enemy.attack_cooldown_timer - delta_time);
}

static void UpdateGohmaLarva(Enemy &enemy, float delta_time, const EnemyUpdateContext &context)
{
    const glm::vec4 delta_to_player = context.player_position - enemy.position;
    const float distance_to_player = DistanceXZ(enemy.position, context.player_position);
    enemy.vulnerable = true;

    switch (enemy.state)
    {
    case EnemyState::Idle:
        if (distance_to_player <= enemy.detection_radius)
        {
            enemy.state = EnemyState::Chasing;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Chasing:
    {
        const glm::vec4 direction = NormalizeXZ(delta_to_player);
        enemy.yaw = ComputeYawToTarget(enemy.position, context.player_position);
        MoveEnemyWithScenarioCollision(enemy, direction * (1.9f * delta_time), *context.scenario_collision_shapes);

        if (distance_to_player <= enemy.attack_radius && enemy.attack_cooldown_timer <= 0.0f)
        {
            enemy.state = EnemyState::Jumping;
            enemy.state_timer = 0.0f;
            enemy.attack_cooldown_timer = enemy.attack_cooldown;
        }
        break;
    }

    case EnemyState::Jumping:
    {
        const glm::vec4 direction = NormalizeXZ(delta_to_player);
        enemy.yaw = ComputeYawToTarget(enemy.position, context.player_position);
        MoveEnemyWithScenarioCollision(enemy, direction * (3.6f * delta_time), *context.scenario_collision_shapes);

        const float jump_progress = Clamp01(enemy.state_timer / GOHMA_LARVA_JUMP_DURATION);
        enemy.position.y = enemy.spawn_position.y + std::sin(jump_progress * PI) * 0.45f;

        if (enemy.state_timer >= GOHMA_LARVA_JUMP_DURATION)
        {
            enemy.position.y = enemy.spawn_position.y;
            enemy.state = EnemyState::Chasing;
            enemy.state_timer = 0.0f;
        }

        if (DistanceXZ(enemy.position, context.player_position) <= enemy.attack_radius + 0.35f &&
            jump_progress >= 0.65f)
            LogPlayerHitByEnemy("Gohma Larva");
        // TODO: Aplicar dano/empurrão ao jogador na aterrissagem da larva.
        break;
    }

    default:
        enemy.state = EnemyState::Idle;
        enemy.state_timer = 0.0f;
        break;
    }

    if (enemy.attack_cooldown_timer > 0.0f)
        enemy.attack_cooldown_timer = std::max(0.0f, enemy.attack_cooldown_timer - delta_time);
}

static void UpdateQueenGohma(Enemy &enemy, float delta_time, const EnemyUpdateContext &context)
{
    const float distance_to_player = DistanceXZ(enemy.position, context.player_position);
    enemy.yaw = ComputeYawToTarget(enemy.position, context.player_position);
    enemy.vulnerable = false;

    switch (enemy.state)
    {
    case EnemyState::BossIdle:
        if (enemy.state_timer >= 1.1f)
        {
            enemy.state = EnemyState::BossLookAtPlayer;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::BossLookAtPlayer:
        enemy.vulnerable = true;
        if (distance_to_player <= enemy.attack_radius && enemy.attack_cooldown_timer <= 0.0f)
        {
            enemy.state = EnemyState::BossAttack;
            enemy.state_timer = 0.0f;
        }
        else if (enemy.state_timer >= 3.8f)
        {
            enemy.state = EnemyState::BossClimb;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::BossAttack:
    {
        const glm::vec4 dash = ForwardFromYaw(enemy.yaw) * (2.1f * delta_time);
        MoveEnemyWithScenarioCollision(enemy, dash, *context.scenario_collision_shapes);
        enemy.vulnerable = (enemy.state_timer >= 0.55f);

        if (enemy.state_timer >= 1.2f)
        {
            enemy.attack_cooldown_timer = 2.5f;
            enemy.state = EnemyState::BossLookAtPlayer;
            enemy.state_timer = 0.0f;
        }

        if (DistanceXZ(enemy.position, context.player_position) <= enemy.attack_radius + 0.80f)
            LogPlayerHitByEnemy("Queen Gohma");
        // TODO: Aplicar dano da investida da Queen Gohma no jogador.
        break;
    }

    case EnemyState::BossClimb:
        enemy.position.y = enemy.spawn_position.y + std::min(2.8f, enemy.state_timer * 1.6f);
        if (enemy.state_timer >= 2.0f)
        {
            enemy.state = EnemyState::BossDropEggs;
            enemy.state_timer = 0.0f;
            enemy.has_spawned_helpers = false;
        }
        break;

    case EnemyState::BossDropEggs:
        enemy.position.y = enemy.spawn_position.y + 2.8f;
        if (!enemy.has_spawned_helpers && enemy.state_timer >= 0.4f)
        {
            SpawnQueenGohmaEggWave(enemy);
            enemy.has_spawned_helpers = true;
        }

        if (enemy.state_timer >= 1.6f)
        {
            enemy.state = EnemyState::BossAttack;
            enemy.state_timer = 0.0f;
            enemy.position.y = enemy.spawn_position.y;
            enemy.has_spawned_helpers = false;
        }
        break;

    case EnemyState::BossStunned:
        enemy.vulnerable = true;
        if (enemy.state_timer >= QUEEN_GOHMA_STUN_DURATION)
        {
            enemy.state = EnemyState::BossLookAtPlayer;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::BossDead:
        enemy.active = false;
        enemy.visible = false;
        break;

    default:
        enemy.state = EnemyState::BossIdle;
        enemy.state_timer = 0.0f;
        break;
    }

    if (enemy.attack_cooldown_timer > 0.0f)
        enemy.attack_cooldown_timer = std::max(0.0f, enemy.attack_cooldown_timer - delta_time);
}

void InitializeEnemies(const glm::vec4 &hardcoded_test_spawn_position)
{
    g_Enemies.clear();
    g_PendingEnemySpawns.clear();
    g_EnemyProjectiles.clear();
    const float test_ground_y = hardcoded_test_spawn_position.y - 4.0f;

    Enemy deku_baba = MakeEnemy(
        EnemyType::DEKU_BABA,
        EnemyState::Idle,
        glm::vec4(4.3f, test_ground_y, 2.4f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f),
        glm::vec4(0.45f, 0.95f, 0.45f, 0.0f),
        2,
        3.8f,
        1.3f,
        1.8f,
        "Dekubaba");
    deku_baba.debug_color = glm::vec4(0.42f, 0.82f, 0.28f, 1.0f);
    g_Enemies.push_back(deku_baba);

    Enemy withered = MakeEnemy(
        EnemyType::WITHERED_DEKU_BABA,
        EnemyState::Idle,
        glm::vec4(0.9f, test_ground_y, 4.6f, 1.0f),
        glm::vec4(0.72f, 0.72f, 0.72f, 0.0f),
        glm::vec4(0.38f, 0.60f, 0.38f, 0.0f),
        1,
        0.0f,
        0.0f,
        0.0f,
        "Dekubaba");
    withered.yaw = 0.35f;
    withered.timer_b = withered.yaw;
    withered.debug_color = glm::vec4(0.70f, 0.62f, 0.38f, 1.0f);
    g_Enemies.push_back(withered);

    Enemy deku_scrub = MakeEnemy(
        EnemyType::DEKU_SCRUB,
        EnemyState::Hidden,
        glm::vec4(6.8f, test_ground_y, -0.3f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f),
        glm::vec4(0.95f, 0.95f, 0.95f, 0.0f),
        2,
        6.2f,
        5.5f,
        1.7f,
        "nodes_12__meshes[2]");
    deku_scrub.debug_color = glm::vec4(0.86f, 0.58f, 0.22f, 1.0f);
    g_Enemies.push_back(deku_scrub);

    Enemy skullwalltula = MakeEnemy(
        EnemyType::SKULLWALLTULA,
        EnemyState::Idle,
        glm::vec4(-1.9f, test_ground_y, 5.2f, 1.0f),
        glm::vec4(1.20f, 1.20f, 1.20f, 0.0f),
        glm::vec4(0.90f, 1.30f, 0.50f, 0.0f),
        2,
        0.0f,
        1.1f,
        0.0f,
        "skullwalltula_placeholder");
    skullwalltula.debug_color = glm::vec4(0.88f, 0.18f, 0.18f, 1.0f);
    g_Enemies.push_back(skullwalltula);

    Enemy big_skulltula = MakeEnemy(
        EnemyType::BIG_SKULLTULA,
        EnemyState::Turning,
        glm::vec4(8.6f, test_ground_y, 5.1f, 1.0f),
        glm::vec4(2.70f, 2.70f, 2.70f, 0.0f),
        glm::vec4(1.40f, 1.50f, 1.40f, 0.0f),
        3,
        0.0f,
        1.8f,
        2.4f,
        "big_skulltula_placeholder");
    big_skulltula.debug_color = glm::vec4(0.96f, 0.58f, 0.14f, 1.0f);
    g_Enemies.push_back(big_skulltula);

    Enemy larva = MakeEnemy(
        EnemyType::GOHMA_LARVA,
        EnemyState::Idle,
        glm::vec4(10.2f, test_ground_y, 2.0f, 1.0f),
        glm::vec4(1.44f, 1.44f, 1.44f, 0.0f),
        glm::vec4(0.64f, 0.40f, 0.64f, 0.0f),
        2,
        5.5f,
        1.0f,
        1.8f,
        "gohma_larva_placeholder");
    larva.vulnerable = true;
    larva.debug_color = glm::vec4(0.25f, 0.82f, 0.36f, 1.0f);
    g_Enemies.push_back(larva);

    Enemy queen_gohma = MakeEnemy(
        EnemyType::QUEEN_GOHMA,
        EnemyState::BossIdle,
        glm::vec4(13.8f, test_ground_y, -1.6f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f),
        glm::vec4(1.45f, 1.25f, 1.45f, 0.0f),
        8,
        12.0f,
        3.2f,
        2.5f,
        "Queen_Gohma");
    queen_gohma.yaw = -PI * 0.5f;
    queen_gohma.debug_color = glm::vec4(0.62f, 0.25f, 0.22f, 1.0f);
    g_Enemies.push_back(queen_gohma);
}

void UpdateEnemy(std::size_t enemy_index, float delta_time, const EnemyUpdateContext &context)
{
    Enemy &enemy = g_Enemies[enemy_index];
    if (!enemy.active || enemy.dead)
        return;

    enemy.state_timer += delta_time;
    enemy.animation_timer += delta_time;

    switch (enemy.type)
    {
    case EnemyType::DEKU_BABA:
        UpdateDekuBaba(enemy, delta_time, context);
        break;
    case EnemyType::WITHERED_DEKU_BABA:
        UpdateWitheredDekuBaba(enemy, delta_time);
        break;
    case EnemyType::DEKU_SCRUB:
        UpdateDekuScrub(enemy, delta_time, context);
        break;
    case EnemyType::SKULLWALLTULA:
        UpdateSkullwalltula(enemy, delta_time, context);
        break;
    case EnemyType::BIG_SKULLTULA:
        UpdateBigSkulltula(enemy, delta_time, context);
        break;
    case EnemyType::GOHMA_LARVA:
        UpdateGohmaLarva(enemy, delta_time, context);
        break;
    case EnemyType::QUEEN_GOHMA:
        UpdateQueenGohma(enemy, delta_time, context);
        break;
    }
}

void UpdateEnemies(float delta_time, const EnemyUpdateContext &context)
{
    if (g_PlayerHitLogCooldown > 0.0f)
        g_PlayerHitLogCooldown = std::max(0.0f, g_PlayerHitLogCooldown - delta_time);

    for (std::size_t i = 0; i < g_Enemies.size(); ++i)
        UpdateEnemy(i, delta_time, context);

    if (!g_PendingEnemySpawns.empty())
    {
        g_Enemies.insert(g_Enemies.end(), g_PendingEnemySpawns.begin(), g_PendingEnemySpawns.end());
        g_PendingEnemySpawns.clear();
    }
}

void UpdateEnemyProjectiles(float delta_time, const EnemyUpdateContext &context)
{
    for (std::size_t i = 0; i < g_EnemyProjectiles.size(); ++i)
    {
        EnemyProjectile &projectile = g_EnemyProjectiles[i];
        if (!projectile.active)
            continue;

        projectile.position += projectile.velocity * delta_time;
        projectile.lifetime_seconds += delta_time;

        const glm::vec4 projectile_half_extents = projectile.scale;
        const CollisionShapeType scenario_hit =
            CollidesWithScenario(projectile.position, *context.scenario_collision_shapes, projectile_half_extents);

        if (projectile.lifetime_seconds >= projectile.max_lifetime_seconds ||
            scenario_hit == CollisionShapeType::SOLID ||
            scenario_hit == CollisionShapeType::DOOR)
        {
            projectile.active = false;
            continue;
        }

        if (BoxesIntersect(context.player_position, context.player_half_extents, projectile.position, projectile_half_extents))
        {
            projectile.active = false;
            LogPlayerHitByEnemy("projétil inimigo");
            // TODO: Integrar dano no jogador e reflexão de escudo para projéteis inimigos.
        }
    }
}

void DrawEnemies(const EnemyDrawContext &context)
{
    for (std::size_t i = 0; i < g_Enemies.size(); ++i)
    {
        const Enemy &enemy = g_Enemies[i];
        if (!enemy.active || enemy.dead || !enemy.visible)
            continue;

        const RenderModelInfo &render_info = GetEnemyRenderInfo(enemy, context.render_resources);
        if (!render_info.available || render_info.object_names.empty())
            continue;

        const glm::mat4 model = BuildEnemyModelMatrix(enemy, context.render_resources);
        glUniformMatrix4fv(context.model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(
            context.object_id_uniform,
            IsEnemyUsingPlaceholder(enemy, context.render_resources) ? context.object_id_sphere : context.object_id_enemy);
        glUniform1i(context.cube_colliding_uniform, 0);
        glUniform3f(context.object_tint_uniform, enemy.debug_color.x, enemy.debug_color.y, enemy.debug_color.z);

        for (std::size_t object_index = 0; object_index < render_info.object_names.size(); ++object_index)
            context.draw_virtual_object(render_info.object_names[object_index].c_str());
    }
}

void DrawEnemyProjectiles(const EnemyDrawContext &context)
{
    const RenderModelInfo &projectile_render_info =
        context.render_resources.deku_scrub_projectile_render_info &&
                context.render_resources.deku_scrub_projectile_render_info->available
            ? *context.render_resources.deku_scrub_projectile_render_info
            : (context.render_resources.sphere_render_info ? *context.render_resources.sphere_render_info : g_EmptyRenderModelInfo);

    if (!projectile_render_info.available || projectile_render_info.object_names.empty())
        return;

    for (std::size_t i = 0; i < g_EnemyProjectiles.size(); ++i)
    {
        const EnemyProjectile &projectile = g_EnemyProjectiles[i];
        if (!projectile.active)
            continue;

        glm::mat4 model =
            Matrix_Translate(projectile.position.x, projectile.position.y, projectile.position.z) *
            Matrix_Scale(
                projectile.scale.x * projectile_render_info.base_scale,
                projectile.scale.y * projectile_render_info.base_scale,
                projectile.scale.z * projectile_render_info.base_scale);

        if (projectile_render_info.available)
        {
            model = model * Matrix_Translate(
                                -projectile_render_info.center.x,
                                -projectile_render_info.center.y,
                                -projectile_render_info.center.z);
        }

        glUniformMatrix4fv(context.model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(
            context.object_id_uniform,
            context.render_resources.deku_scrub_projectile_render_info &&
                    context.render_resources.deku_scrub_projectile_render_info->available
                ? context.object_id_scenario
                : context.object_id_projectile);
        glUniform1i(context.cube_colliding_uniform, 0);
        glUniform3f(context.object_tint_uniform, 0.85f, 0.68f, 0.30f);

        for (std::size_t object_index = 0; object_index < projectile_render_info.object_names.size(); ++object_index)
            context.draw_virtual_object(projectile_render_info.object_names[object_index].c_str());
    }
}

int QueryEnemyHitByPlayerProjectile(const glm::vec4 &projectile_position, float projectile_radius)
{
    const glm::vec4 projectile_half_extents(projectile_radius, projectile_radius, projectile_radius, 0.0f);

    for (std::size_t i = 0; i < g_Enemies.size(); ++i)
    {
        const Enemy &enemy = g_Enemies[i];
        if (!enemy.active || enemy.dead || !enemy.visible)
            continue;

        if (EnemyAabbIntersectsPointBox(enemy, projectile_position, projectile_half_extents))
            return static_cast<int>(i);
    }

    return -1;
}

void ApplyPlayerProjectileDamageToEnemy(int enemy_index, int damage)
{
    if (enemy_index < 0 || enemy_index >= static_cast<int>(g_Enemies.size()))
        return;

    ApplyDamageToEnemy(g_Enemies[enemy_index], damage);
}
