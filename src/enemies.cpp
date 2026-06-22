#include "enemies.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>

#include <glm/gtc/type_ptr.hpp>

#include "effects.h"
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
static const float DEKU_SCRUB_PLANT_Y_OFFSET = -0.55f;
static const float DEKU_SCRUB_STUN_DURATION = 3.0f;
static const float SKULLWALLTULA_SCALE = 1.2f;
static const float SKULLWALLTULA_VERTICAL_RANGE = 1.0f;
static const float SKULLWALLTULA_SPEED = 0.3f;
static const float SKULLWALLTULA_CONTACT_RADIUS_BONUS = 0.04f;
static const float SKULLWALLTULA_ALERT_RADIUS = 1.9f;
static const float SKULLWALLTULA_ATTACK_TILT = 0.22f;
static const float BIG_SKULLTULA_SCALE = 2.8f;
static const float BIG_SKULLTULA_TURN_SPEED = 2.4f;
static const float BIG_SKULLTULA_ATTACK_RADIUS = 2.0f;
static const float BIG_SKULLTULA_ATTACK_SPEED = 1.9f;
static const float BIG_SKULLTULA_ATTACK_DURATION = 0.85f;
static const float BIG_SKULLTULA_COOLDOWN = 2.5f;
static const float BIG_SKULLTULA_RECOVERY_DURATION = 0.55f;
static const float BIG_SKULLTULA_VULNERABLE_DURATION = 1.6f;
static const float BIG_SKULLTULA_BACK_VULNERABLE_DOT = -0.30f;
static const float BIG_SKULLTULA_FRONT_PROTECTED_DOT = -0.05f;
static const float BIG_SKULLTULA_CONTACT_RADIUS_BONUS = 0.22f;
static const float BIG_SKULLTULA_ATTACK_TILT = 0.32f;
static const float BIG_SKULLTULA_RECOVERY_TILT = -0.18f;
static const float BIG_SKULLTULA_IDLE_TILT_SWAY = 0.05f;
static const float GOHMA_LARVA_JUMP_DURATION = 0.7f;
static const float QUEEN_GOHMA_STUN_DURATION = 4.0f;
static const float ENEMY_DEATH_DURATION = 1.15f;
static const float ENEMY_DEATH_GRAVITY = 7.8f;

static const RenderModelInfo g_EmptyRenderModelInfo = {false, {}, glm::vec4(0.0f), 1.0f};
static const char *g_EnemySpawnScriptPathFromBin = "../../data/enemy_spawns.json";
static const char *g_EnemySpawnScriptPathFromRoot = "data/enemy_spawns.json";

struct ScriptEnemySpawn
{
    std::string type;
    glm::vec4 position;
};

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

static std::string ResolveEnemySpawnScriptPath()
{
    std::ifstream test_bin(g_EnemySpawnScriptPathFromBin);
    if (test_bin.good())
        return std::string(g_EnemySpawnScriptPathFromBin);

    std::ifstream test_root(g_EnemySpawnScriptPathFromRoot);
    if (test_root.good())
        return std::string(g_EnemySpawnScriptPathFromRoot);

    return std::string(g_EnemySpawnScriptPathFromBin);
}

static std::string LoadTextFile(const std::string &path)
{
    std::ifstream file(path.c_str());
    if (!file.is_open())
        return std::string();

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static std::size_t FindMatchingDelimiter(const std::string &text, std::size_t open_pos, char open_char, char close_char)
{
    if (open_pos == std::string::npos || open_pos >= text.size() || text[open_pos] != open_char)
        return std::string::npos;

    int depth = 0;
    for (std::size_t i = open_pos; i < text.size(); ++i)
    {
        if (text[i] == open_char)
            ++depth;
        else if (text[i] == close_char)
        {
            --depth;
            if (depth == 0)
                return i;
        }
    }

    return std::string::npos;
}

static std::vector<ScriptEnemySpawn> LoadEnemySpawnsForArea(const std::string &area_id)
{
    const std::string script_path = ResolveEnemySpawnScriptPath();
    const std::string json_text = LoadTextFile(script_path);
    if (json_text.empty())
    {
        std::fprintf(stderr, "[EnemySpawn] Failed to read spawn script: %s\n", script_path.c_str());
        return {};
    }

    const std::string area_token = "\"id\": \"" + area_id + "\"";
    const std::size_t area_pos = json_text.find(area_token);
    if (area_pos == std::string::npos)
    {
        std::fprintf(stderr, "[EnemySpawn] Area '%s' not found in %s\n", area_id.c_str(), script_path.c_str());
        return {};
    }

    const std::size_t enemies_key_pos = json_text.find("\"enemies\"", area_pos);
    if (enemies_key_pos == std::string::npos)
    {
        std::fprintf(stderr, "[EnemySpawn] Area '%s' is missing an enemies array in %s\n", area_id.c_str(), script_path.c_str());
        return {};
    }

    const std::size_t array_start = json_text.find('[', enemies_key_pos);
    const std::size_t array_end = FindMatchingDelimiter(json_text, array_start, '[', ']');
    if (array_start == std::string::npos || array_end == std::string::npos || array_end <= array_start)
    {
        std::fprintf(stderr, "[EnemySpawn] Area '%s' has an invalid enemies array in %s\n", area_id.c_str(), script_path.c_str());
        return {};
    }

    const std::string enemies_block = json_text.substr(array_start + 1, array_end - array_start - 1);

    std::vector<ScriptEnemySpawn> spawns;
    std::size_t entry_pos = 0;
    while (true)
    {
        const std::size_t type_key_pos = enemies_block.find("\"type\"", entry_pos);
        if (type_key_pos == std::string::npos)
            break;

        const std::size_t type_value_start = enemies_block.find('"', type_key_pos + 6);
        const std::size_t type_value_end = enemies_block.find('"', type_value_start + 1);
        const std::size_t position_key_pos = enemies_block.find("\"position\"", type_value_end);
        const std::size_t position_value_start = enemies_block.find('[', position_key_pos);
        const std::size_t position_value_end = enemies_block.find(']', position_value_start);

        if (type_value_start == std::string::npos ||
            type_value_end == std::string::npos ||
            position_key_pos == std::string::npos ||
            position_value_start == std::string::npos ||
            position_value_end == std::string::npos)
        {
            break;
        }

        std::string position_values = enemies_block.substr(position_value_start + 1, position_value_end - position_value_start - 1);
        std::replace(position_values.begin(), position_values.end(), ',', ' ');
        std::stringstream position_stream(position_values);

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        if (!(position_stream >> x >> y >> z))
        {
            entry_pos = position_value_end + 1;
            continue;
        }

        ScriptEnemySpawn spawn;
        spawn.type = enemies_block.substr(type_value_start + 1, type_value_end - type_value_start - 1);
        spawn.position = glm::vec4(x, y, z, 1.0f);
        spawns.push_back(spawn);

        entry_pos = position_value_end + 1;
    }

    if (spawns.empty())
    {
        std::fprintf(stderr, "[EnemySpawn] Area '%s' has no valid enemy entries in %s\n", area_id.c_str(), script_path.c_str());
    }
    else
    {
        std::fprintf(stdout, "[EnemySpawn] Loaded %zu spawns for area '%s' from %s\n", spawns.size(), area_id.c_str(), script_path.c_str());
        for (std::size_t i = 0; i < spawns.size(); ++i)
        {
            std::fprintf(
                stdout,
                "[EnemySpawn]   %s at (%.2f, %.2f, %.2f)\n",
                spawns[i].type.c_str(),
                spawns[i].position.x,
                spawns[i].position.y,
                spawns[i].position.z);
        }
    }

    return spawns;
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
    enemy.death_effect_spawned = false;
    enemy.death_timer = 0.0f;
    enemy.death_start_position = position;
    enemy.death_velocity = glm::vec4(0.0f);
    enemy.death_rotation = 0.0f;
    enemy.debug_color = glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);
    enemy.object_name = object_name;
    return enemy;
}

static bool TryCreateEnemyFromScriptSpawn(const ScriptEnemySpawn &spawn, Enemy &out_enemy)
{
    if (spawn.type == "baba")
    {
        out_enemy = MakeEnemy(
            EnemyType::DEKU_BABA,
            EnemyState::Idle,
            spawn.position,
            glm::vec4(1.0f, 1.0f, 1.0f, 0.0f),
            glm::vec4(0.45f, 0.95f, 0.45f, 0.0f),
            2,
            3.8f,
            1.3f,
            1.8f,
            "Dekubaba");
        out_enemy.debug_color = glm::vec4(0.42f, 0.82f, 0.28f, 1.0f);
        return true;
    }

    if (spawn.type == "small spider")
    {
        out_enemy = MakeEnemy(
            EnemyType::SKULLWALLTULA,
            EnemyState::Idle,
            spawn.position,
            glm::vec4(SKULLWALLTULA_SCALE, SKULLWALLTULA_SCALE, SKULLWALLTULA_SCALE, 0.0f),
            glm::vec4(0.58f, 0.76f, 0.58f, 0.0f),
            2,
            0.0f,
            1.1f,
            0.0f,
            "Only_Spider_with_Animations_Export");
        out_enemy.timer_a = 0.35f;
        out_enemy.timer_b = PI;
        out_enemy.blocks_movement = true;
        out_enemy.debug_color = glm::vec4(0.72f, 0.72f, 0.72f, 1.0f);
        return true;
    }

    return false;
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
    case EnemyType::SKULLWALLTULA:
    case EnemyType::BIG_SKULLTULA:
        return render_resources.spider_render_info ? *render_resources.spider_render_info : g_EmptyRenderModelInfo;
    case EnemyType::QUEEN_GOHMA:
        return render_resources.queen_gohma_render_info ? *render_resources.queen_gohma_render_info : g_EmptyRenderModelInfo;
    case EnemyType::GOHMA_LARVA:
    default:
        return render_resources.sphere_render_info ? *render_resources.sphere_render_info : g_EmptyRenderModelInfo;
    }
}

static bool IsEnemyUsingPlaceholder(const Enemy &enemy, const EnemyRenderResources &render_resources)
{
    return !GetEnemyRenderInfo(enemy, render_resources).available ||
           enemy.type == EnemyType::GOHMA_LARVA;
}

static glm::mat4 BuildEnemyModelMatrix(const Enemy &enemy, const EnemyRenderResources &render_resources)
{
    const RenderModelInfo &render_info = GetEnemyRenderInfo(enemy, render_resources);
    glm::mat4 model =
        Matrix_Translate(enemy.position.x, enemy.position.y, enemy.position.z) *
        Matrix_Rotate_Y(enemy.yaw);

    if (enemy.type == EnemyType::SKULLWALLTULA || enemy.type == EnemyType::BIG_SKULLTULA)
        model = model * Matrix_Rotate_X(-PI * 0.5f);

    if (std::fabs(enemy.pitch) > 1e-4f)
        model = model * Matrix_Rotate_X(enemy.pitch);

    if (enemy.state == EnemyState::Dying && std::fabs(enemy.death_rotation) > 1e-4f)
        model = model * Matrix_Rotate_Z(enemy.death_rotation);

    model = model *
            Matrix_Scale(
                enemy.scale.x * render_info.base_scale,
                enemy.scale.y * render_info.base_scale,
                enemy.scale.z * render_info.base_scale);

    if (render_info.available)
        model = model * Matrix_Translate(-render_info.center.x, -render_info.center.y, -render_info.center.z);

    return model;
}

static glm::mat4 BuildAnchoredModelMatrix(
    const glm::vec4 &position,
    const glm::vec4 &scale,
    const RenderModelInfo &render_info)
{
    glm::mat4 model =
        Matrix_Translate(position.x, position.y, position.z) *
        Matrix_Scale(
            scale.x * render_info.base_scale,
            scale.y * render_info.base_scale,
            scale.z * render_info.base_scale);

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
        expanded_half_extents.x += 0.04f;
        expanded_half_extents.y += 0.04f;
        expanded_half_extents.z += 0.04f;
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

static bool EnemyBlocksPlayerMovement(const Enemy &enemy)
{
    return enemy.active && !enemy.dead && enemy.visible &&
           enemy.state != EnemyState::Dying && enemy.blocks_movement;
}

static bool WouldBlockingEnemyOverlapPlayer(
    const Enemy &enemy,
    const glm::vec4 &enemy_position,
    const glm::vec4 &player_position,
    const glm::vec4 &player_half_extents)
{
    return EnemyBlocksPlayerMovement(enemy) &&
           BoxesIntersect(enemy_position, enemy.collision_half_extents, player_position, player_half_extents);
}

static void MoveEnemyWithScenarioCollision(
    Enemy &enemy,
    const glm::vec4 &movement,
    const std::vector<CollisionShape> &scenario_collision_shapes,
    const glm::vec4 &player_position,
    const glm::vec4 &player_half_extents)
{
    glm::vec4 updated_position = enemy.position;
    glm::vec4 test_half_extents = enemy.collision_half_extents;
    test_half_extents.y = std::max(0.08f, test_half_extents.y - ENEMY_COLLISION_MARGIN);

    glm::vec4 test_position_x = updated_position;
    test_position_x.x += movement.x;
    CollisionShapeType collision_x = CollidesWithScenario(test_position_x, scenario_collision_shapes, test_half_extents);
    if (collision_x != CollisionShapeType::SOLID &&
        collision_x != CollisionShapeType::DOOR &&
        !WouldBlockingEnemyOverlapPlayer(enemy, test_position_x, player_position, player_half_extents))
        updated_position.x = test_position_x.x;

    glm::vec4 test_position_z = updated_position;
    test_position_z.z += movement.z;
    CollisionShapeType collision_z = CollidesWithScenario(test_position_z, scenario_collision_shapes, test_half_extents);
    if (collision_z != CollisionShapeType::SOLID &&
        collision_z != CollisionShapeType::DOOR &&
        !WouldBlockingEnemyOverlapPlayer(enemy, test_position_z, player_position, player_half_extents))
        updated_position.z = test_position_z.z;

    enemy.position = updated_position;
}

static bool IsWallMountedEnemy(const Enemy &enemy)
{
    return enemy.type == EnemyType::SKULLWALLTULA;
}

static float GetApproximateDeathGroundY(const Enemy &enemy)
{
    if (enemy.type == EnemyType::SKULLWALLTULA)
    {
        // TODO: Substituir por amostragem real do chao quando a colisao expuser altura precisa.
        return enemy.spawn_position.y - (SKULLWALLTULA_VERTICAL_RANGE + 0.85f);
    }

    return enemy.spawn_position.y - 0.12f;
}

static void FinalizeEnemyDeath(Enemy &enemy)
{
    if (!enemy.death_effect_spawned)
    {
        SpawnSmokeBurst(enemy.position + glm::vec4(0.0f, enemy.collision_half_extents.y * 0.30f, 0.0f, 0.0f));
        enemy.death_effect_spawned = true;
    }

    enemy.dead = true;
    enemy.active = false;
    enemy.visible = false;
    enemy.vulnerable = false;
    enemy.state = (enemy.type == EnemyType::QUEEN_GOHMA) ? EnemyState::BossDead : EnemyState::Dead;

    // TODO: Integrar drops (Deku Stick, Deku Nut e recompensas do chefe).
}

static void BeginEnemyDeath(Enemy &enemy)
{
    if (!enemy.active || enemy.dead || enemy.state == EnemyState::Dying)
        return;

    enemy.state = EnemyState::Dying;
    enemy.state_timer = 0.0f;
    enemy.death_timer = 0.0f;
    enemy.death_start_position = enemy.position;
    enemy.death_rotation = 0.0f;
    enemy.death_effect_spawned = false;
    enemy.vulnerable = false;
    enemy.blocks_movement = false;
    enemy.has_spawned_helpers = false;

    if (IsWallMountedEnemy(enemy))
    {
        const glm::vec4 outward = ForwardFromYaw(enemy.yaw);
        enemy.death_velocity = outward * 0.55f + glm::vec4(0.0f, 1.55f, 0.0f, 0.0f);
    }
    else
    {
        const glm::vec4 backward = -ForwardFromYaw(enemy.yaw) * 0.18f;
        enemy.death_velocity = backward;
    }

    // A fumaça aparece no momento em que o corpo some, para reforçar o desaparecimento.
}

static void UpdateEnemyDying(Enemy &enemy, float delta_time)
{
    enemy.death_timer += delta_time;

    if (IsWallMountedEnemy(enemy))
    {
        enemy.death_velocity.y -= ENEMY_DEATH_GRAVITY * delta_time;
        enemy.position += enemy.death_velocity * delta_time;
        enemy.pitch -= 4.0f * delta_time;
        enemy.death_rotation += 6.0f * delta_time;

        const float ground_y = GetApproximateDeathGroundY(enemy);
        if (enemy.position.y <= ground_y)
        {
            enemy.position.y = ground_y;
            enemy.death_velocity = glm::vec4(0.0f);
        }
    }
    else
    {
        const float death_progress = Clamp01(enemy.death_timer / ENEMY_DEATH_DURATION);
        enemy.position += enemy.death_velocity * delta_time;
        enemy.position.y = enemy.death_start_position.y - 0.18f * death_progress;
        enemy.pitch = -1.10f * death_progress;
        enemy.death_rotation = 0.75f * death_progress;
    }

    if (enemy.death_timer >= ENEMY_DEATH_DURATION)
        FinalizeEnemyDeath(enemy);
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
        BeginEnemyDeath(enemy);
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
    const glm::vec4 forward = NormalizeXZ(direction_to_player);
    const glm::vec4 velocity = forward * 6.5f;
    const glm::vec4 spawn_offset =
        forward * 0.58f +
        glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

    SpawnEnemyProjectile(
        EnemyProjectileType::DEKU_NUT,
        enemy.position + spawn_offset,
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
    enemy.position.x = enemy.spawn_position.x;
    enemy.position.z = enemy.spawn_position.z;
    enemy.yaw = enemy.timer_b;
    enemy.vulnerable = true;
    enemy.pitch = 0.0f;
    const float distance_to_player = DistanceXZ(enemy.position, context.player_position);

    switch (enemy.state)
    {
    case EnemyState::Idle:
        if (enemy.state_timer >= 0.2f)
        {
            enemy.state = EnemyState::Patrol;
            enemy.state_timer = 0.0f;
        }
        enemy.position.y = enemy.spawn_position.y;
        break;

    case EnemyState::Patrol:
        enemy.position.y =
            enemy.spawn_position.y +
            std::sin(enemy.animation_timer * SKULLWALLTULA_SPEED + enemy.timer_a) * SKULLWALLTULA_VERTICAL_RANGE;
        break;

    default:
        enemy.state = EnemyState::Idle;
        enemy.state_timer = 0.0f;
        enemy.position.y = enemy.spawn_position.y;
        break;
    }

    if (distance_to_player <= SKULLWALLTULA_ALERT_RADIUS)
    {
        const float proximity = 1.0f - Clamp01(distance_to_player / SKULLWALLTULA_ALERT_RADIUS);
        enemy.pitch = SKULLWALLTULA_ATTACK_TILT * proximity;
    }

    if (EnemyAabbIntersectsPointBox(
            enemy,
            context.player_position,
            context.player_half_extents + glm::vec4(SKULLWALLTULA_CONTACT_RADIUS_BONUS)))
    {
        LogPlayerHitByEnemy("Skullwalltula");
        // TODO: Integrar dano/knockback ao jogador e interacao com o sistema de escalada.
    }
}

static void UpdateBigSkulltula(Enemy &enemy, float delta_time, const EnemyUpdateContext &context)
{
    const glm::vec4 raw_to_player = context.player_position - enemy.position;
    const glm::vec4 to_player = NormalizeXZ(raw_to_player);
    const glm::vec4 enemy_forward = ForwardFromYaw(enemy.yaw);
    const float front_dot = dotproduct(enemy_forward, to_player);
    const float distance_to_player = DistanceXZ(enemy.position, context.player_position);
    const float target_yaw = ComputeYawToTarget(enemy.position, context.player_position);

    enemy.vulnerable = false;
    enemy.pitch = BIG_SKULLTULA_IDLE_TILT_SWAY * std::sin(enemy.animation_timer * 1.8f);

    switch (enemy.state)
    {
    case EnemyState::Turning:
    case EnemyState::Idle:
        enemy.state = EnemyState::Turning;
        enemy.yaw = SmoothFollowAngle(enemy.yaw, target_yaw, BIG_SKULLTULA_TURN_SPEED, delta_time);
        if (distance_to_player <= BIG_SKULLTULA_ATTACK_RADIUS && enemy.attack_cooldown_timer <= 0.0f)
        {
            enemy.state = EnemyState::Attacking;
            enemy.state_timer = 0.0f;
        }
        else if (front_dot <= BIG_SKULLTULA_BACK_VULNERABLE_DOT)
        {
            enemy.state = EnemyState::Vulnerable;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Recovering:
        enemy.pitch = BIG_SKULLTULA_RECOVERY_TILT;
        if (enemy.state_timer >= BIG_SKULLTULA_RECOVERY_DURATION)
        {
            enemy.state = EnemyState::Vulnerable;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Vulnerable:
        enemy.vulnerable = true;
        enemy.pitch = BIG_SKULLTULA_RECOVERY_TILT * 0.5f;
        if (enemy.state_timer >= BIG_SKULLTULA_VULNERABLE_DURATION)
        {
            enemy.state = EnemyState::Turning;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Attacking:
    {
        const float attack_progress = Clamp01(enemy.state_timer / BIG_SKULLTULA_ATTACK_DURATION);
        enemy.pitch = BIG_SKULLTULA_ATTACK_TILT * std::sin(attack_progress * PI);
        enemy.yaw = SmoothFollowAngle(enemy.yaw, target_yaw, BIG_SKULLTULA_TURN_SPEED * 1.35f, delta_time);
        const glm::vec4 dash = ForwardFromYaw(enemy.yaw) * (BIG_SKULLTULA_ATTACK_SPEED * delta_time);
        MoveEnemyWithScenarioCollision(
            enemy,
            dash,
            *context.scenario_collision_shapes,
            context.player_position,
            context.player_half_extents);

        if (enemy.state_timer >= BIG_SKULLTULA_ATTACK_DURATION)
        {
            enemy.attack_cooldown_timer = enemy.attack_cooldown;
            enemy.state = EnemyState::Recovering;
            enemy.state_timer = 0.0f;
        }

        if (EnemyAabbIntersectsPointBox(
                enemy,
                context.player_position,
                context.player_half_extents + glm::vec4(BIG_SKULLTULA_CONTACT_RADIUS_BONUS)))
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
        MoveEnemyWithScenarioCollision(
            enemy,
            direction * (1.9f * delta_time),
            *context.scenario_collision_shapes,
            context.player_position,
            context.player_half_extents);

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
        MoveEnemyWithScenarioCollision(
            enemy,
            direction * (3.6f * delta_time),
            *context.scenario_collision_shapes,
            context.player_position,
            context.player_half_extents);

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
        MoveEnemyWithScenarioCollision(
            enemy,
            dash,
            *context.scenario_collision_shapes,
            context.player_position,
            context.player_half_extents);
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

void InitializeEnemies(const std::string &area_id)
{
    g_Enemies.clear();
    g_PendingEnemySpawns.clear();
    g_EnemyProjectiles.clear();
    const std::vector<ScriptEnemySpawn> spawns = LoadEnemySpawnsForArea(area_id);
    for (std::size_t i = 0; i < spawns.size(); ++i)
    {
        Enemy enemy;
        if (!TryCreateEnemyFromScriptSpawn(spawns[i], enemy))
        {
            std::fprintf(stderr, "[EnemySpawn] Unsupported enemy type '%s' in area '%s'\n", spawns[i].type.c_str(), area_id.c_str());
            continue;
        }

        g_Enemies.push_back(enemy);
    }

    std::fprintf(stdout, "[EnemySpawn] Instantiated %zu enemies for area '%s'\n", g_Enemies.size(), area_id.c_str());
}

void UpdateEnemy(std::size_t enemy_index, float delta_time, const EnemyUpdateContext &context)
{
    Enemy &enemy = g_Enemies[enemy_index];
    if (!enemy.active || enemy.dead)
        return;

    enemy.state_timer += delta_time;
    enemy.animation_timer += delta_time;

    if (enemy.state == EnemyState::Dying)
    {
        UpdateEnemyDying(enemy, delta_time);
        return;
    }

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

        if (enemy.type == EnemyType::DEKU_SCRUB &&
            context.render_resources.deku_scrub_plant_render_info &&
            context.render_resources.deku_scrub_plant_render_info->available &&
            !context.render_resources.deku_scrub_plant_render_info->object_names.empty())
        {
            const RenderModelInfo &plant_render_info = *context.render_resources.deku_scrub_plant_render_info;
            const glm::vec4 plant_position =
                enemy.spawn_position + glm::vec4(0.0f, DEKU_SCRUB_PLANT_Y_OFFSET, 0.0f, 0.0f);
            const glm::mat4 plant_model = BuildAnchoredModelMatrix(plant_position, enemy.scale, plant_render_info);
            glUniformMatrix4fv(context.model_uniform, 1, GL_FALSE, glm::value_ptr(plant_model));
            glUniform1i(context.object_id_uniform, context.object_id_enemy);
            glUniform1i(context.cube_colliding_uniform, 0);
            glUniform3f(context.object_tint_uniform, 1.0f, 1.0f, 1.0f);

            glDisable(GL_CULL_FACE);
            for (std::size_t object_index = 0; object_index < plant_render_info.object_names.size(); ++object_index)
                context.draw_virtual_object(plant_render_info.object_names[object_index].c_str());
            glEnable(GL_CULL_FACE);
        }

        const glm::mat4 model = BuildEnemyModelMatrix(enemy, context.render_resources);
        const bool using_placeholder = IsEnemyUsingPlaceholder(enemy, context.render_resources);
        const bool use_enemy_tint =
            using_placeholder ||
            enemy.type == EnemyType::SKULLWALLTULA ||
            enemy.type == EnemyType::BIG_SKULLTULA;
        glUniformMatrix4fv(context.model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(
            context.object_id_uniform,
            using_placeholder ? context.object_id_sphere : context.object_id_enemy);
        glUniform1i(context.cube_colliding_uniform, 0);
        glUniform3f(
            context.object_tint_uniform,
            use_enemy_tint ? enemy.debug_color.x : 1.0f,
            use_enemy_tint ? enemy.debug_color.y : 1.0f,
            use_enemy_tint ? enemy.debug_color.z : 1.0f);

        if (enemy.type == EnemyType::DEKU_SCRUB)
            glDisable(GL_CULL_FACE);

        for (std::size_t object_index = 0; object_index < render_info.object_names.size(); ++object_index)
            context.draw_virtual_object(render_info.object_names[object_index].c_str());

        if (enemy.type == EnemyType::DEKU_SCRUB)
            glEnable(GL_CULL_FACE);
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
        if (!enemy.active || enemy.dead || !enemy.visible || enemy.state == EnemyState::Dying)
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

bool QueryBlockingEnemyCollision(const glm::vec4 &center, const glm::vec4 &half_extents)
{
    for (std::size_t i = 0; i < g_Enemies.size(); ++i)
    {
        const Enemy &enemy = g_Enemies[i];
        if (!EnemyBlocksPlayerMovement(enemy))
            continue;

        if (BoxesIntersect(center, half_extents, enemy.position, enemy.collision_half_extents))
            return true;
    }

    return false;
}

int QuerySwordHitEnemy(const glm::vec4 &sword_box_center, const glm::vec4 &sword_box_half_extents)
{
    for (std::size_t i = 0; i < g_Enemies.size(); ++i)
    {
        const Enemy &enemy = g_Enemies[i];
        if (!enemy.active || enemy.dead || !enemy.visible || enemy.state == EnemyState::Dying)
            continue;

        if (BoxesIntersect(sword_box_center, sword_box_half_extents, enemy.position, enemy.collision_half_extents))
            return static_cast<int>(i);
    }

    return -1;
}

std::size_t GetEnemyCount()
{
    return g_Enemies.size();
}
