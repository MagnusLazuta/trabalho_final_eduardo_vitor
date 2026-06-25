#ifndef _ENEMIES_H
#define _ENEMIES_H

#include <cstddef>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/vec4.hpp>

#include "collision.h"

enum class EnemyType
{
    DEKU_BABA,
    WITHERED_DEKU_BABA,
    DEKU_SCRUB,
    SKULLWALLTULA,
    BIG_SKULLTULA,
    GOHMA_LARVA,
    QUEEN_GOHMA
};

enum class EnemyState
{
    Idle,
    Patrol,
    Hidden,
    Aiming,
    Shooting,
    Stunned,
    Turning,
    Vulnerable,
    Attacking,
    Recovering,
    Chasing,
    Jumping,
    Dying,
    Dead,
    BossIdle,
    BossLookAtPlayer,
    BossStunned,
    BossClimb,
    BossDropEggs,
    BossAttack,
    BossDead
};

enum class EnemyProjectileType
{
    DEKU_NUT,
    GOHMA_SPIT
};

struct Enemy
{
    EnemyType type;
    EnemyState state;
    glm::vec4 position;
    glm::vec4 spawn_position;
    glm::vec4 velocity;
    glm::vec4 scale;
    glm::vec4 collision_half_extents;
    float yaw;
    float pitch;
    int health;
    float detection_radius;
    float attack_radius;
    float attack_cooldown;
    float attack_cooldown_timer;
    float state_timer;
    float animation_timer;
    float timer_a;
    float timer_b;
    bool active;
    bool dead;
    bool vulnerable;
    bool visible;
    bool blocks_movement;
    bool has_spawned_helpers;
    bool death_effect_spawned;
    float death_timer;
    glm::vec4 death_start_position;
    glm::vec4 death_velocity;
    float death_rotation;
    float hit_flash_timer;
    glm::vec4 debug_color;
    std::string object_name;
};

struct EnemyProjectile
{
    EnemyProjectileType type;
    glm::vec4 position;
    glm::vec4 velocity;
    glm::vec4 source_position;
    glm::vec4 scale;
    float lifetime_seconds;
    float max_lifetime_seconds;
    bool active;
    bool reflected_by_player;
};

struct RenderModelInfo
{
    bool available;
    std::vector<std::string> object_names;
    glm::vec4 center;
    float base_scale;
};

struct EnemyUpdateContext
{
    glm::vec4 player_position;
    glm::vec4 player_half_extents;
    float player_yaw;
    bool player_defending;
    const std::vector<CollisionShape> *scenario_collision_shapes;
};

struct EnemyRenderResources
{
    const RenderModelInfo *deku_baba_render_info;
    const RenderModelInfo *deku_scrub_render_info;
    const RenderModelInfo *deku_scrub_plant_render_info;
    const RenderModelInfo *deku_scrub_projectile_render_info;
    const RenderModelInfo *spider_render_info;
    const RenderModelInfo *queen_gohma_render_info;
    const RenderModelInfo *sphere_render_info;
};

struct EnemyDrawContext
{
    GLint model_uniform;
    GLint object_id_uniform;
    GLint cube_colliding_uniform;
    GLint object_tint_uniform;
    int object_id_scenario;
    int object_id_sphere;
    int object_id_projectile;
    int object_id_enemy;
    EnemyRenderResources render_resources;
    void (*draw_virtual_object)(const char *object_name);
};

void InitializeEnemies(const std::string &area_id);
void UpdateEnemies(float delta_time, const EnemyUpdateContext &context);
void UpdateEnemy(std::size_t enemy_index, float delta_time, const EnemyUpdateContext &context);
void DrawEnemies(const EnemyDrawContext &context);
void UpdateEnemyProjectiles(float delta_time, const EnemyUpdateContext &context);
void DrawEnemyProjectiles(const EnemyDrawContext &context);

int QueryEnemyHitByPlayerProjectile(const glm::vec4 &projectile_position, float projectile_radius);
void ApplyPlayerProjectileDamageToEnemy(int enemy_index, int damage);
bool QueryBlockingEnemyCollision(const glm::vec4 &center, const glm::vec4 &half_extents);
int QuerySwordHitEnemy(const CollisionOBB &sword_obb);
int QueryClosestLockOnEnemy(const glm::vec4 &player_position, float radius, float max_height_delta);
bool QueryEnemyLockOnTargetPosition(int enemy_index, const glm::vec4 &player_position, float radius, float max_height_delta, glm::vec4 &target_position);
int QueryNextLockOnEnemy(const glm::vec4 &player_position, float radius, float max_height_delta, int current_index, int direction);
std::size_t GetEnemyCount();

#endif // _ENEMIES_H
