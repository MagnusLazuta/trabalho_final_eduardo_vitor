#include "effects.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <glm/gtc/type_ptr.hpp>

#include "matrices.h"

enum ParticleEffectType
{
    PARTICLE_EFFECT_SMOKE = 0
};

struct AnimatedImpactEffect
{
    bool active;
    glm::vec4 position;
    float lifetime;
    float max_lifetime;
    float size;
    glm::vec4 color;
};

struct AnimatedSmokeEffect
{
    bool active;
    glm::vec4 position;
    float lifetime;
    float max_lifetime;
    float size;
    glm::vec4 color;
};

static const int MAX_PARTICLES = 256;
static const int MAX_IMPACT_EFFECTS = 24;
static const int MAX_SMOKE_EFFECTS = 24;
static const float SMOKE_LIFETIME = 0.72f;
static const float SMOKE_SIZE = 1.15f;
static const float SLINGSHOT_IMPACT_LIFETIME = 0.86f;
static const float SLINGSHOT_IMPACT_SIZE = 2.50f;
static const float PARTICLE_DRAG = 0.88f;
static const glm::vec4 SMOKE_COLOR_A(0.63f, 0.23f, 0.21f, 0.42f);
static const glm::vec4 IMPACT_COLOR_A(0.98f, 0.86f, 0.34f, 0.45f);

static std::vector<Particle> g_Particles(MAX_PARTICLES);
static std::vector<AnimatedSmokeEffect> g_SmokeEffects(MAX_SMOKE_EFFECTS);
static std::vector<AnimatedImpactEffect> g_ImpactEffects(MAX_IMPACT_EFFECTS);
static GLuint g_ParticleQuadVAO = 0;
static GLuint g_ParticleQuadVBO = 0;
static GLuint g_ParticleQuadEBO = 0;

static void EnsureParticleQuad()
{
    if (g_ParticleQuadVAO != 0)
        return;

    const float vertices[] = {
        -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
    };
    const GLuint indices[] = {0, 1, 2, 0, 2, 3};

    glGenVertexArrays(1, &g_ParticleQuadVAO);
    glBindVertexArray(g_ParticleQuadVAO);

    glGenBuffers(1, &g_ParticleQuadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, g_ParticleQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &g_ParticleQuadEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ParticleQuadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void *)(4 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void *)(8 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static float Random01()
{
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

static float RandomRange(float min_value, float max_value)
{
    return min_value + (max_value - min_value) * Random01();
}

static float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static Particle *GetInactiveParticle()
{
    for (std::size_t i = 0; i < g_Particles.size(); ++i)
    {
        if (!g_Particles[i].active)
            return &g_Particles[i];
    }

    return NULL;
}

static AnimatedImpactEffect *GetInactiveImpactEffect()
{
    for (std::size_t i = 0; i < g_ImpactEffects.size(); ++i)
    {
        if (!g_ImpactEffects[i].active)
            return &g_ImpactEffects[i];
    }

    return NULL;
}

static AnimatedSmokeEffect *GetInactiveSmokeEffect()
{
    for (std::size_t i = 0; i < g_SmokeEffects.size(); ++i)
    {
        if (!g_SmokeEffects[i].active)
            return &g_SmokeEffects[i];
    }

    return NULL;
}

static void SpawnParticle(
    const glm::vec4 &position,
    const glm::vec4 &velocity,
    const glm::vec4 &color,
    float lifetime,
    float start_size,
    float end_size,
    int effect_type)
{
    Particle *particle = GetInactiveParticle();
    if (particle == NULL)
        return;

    particle->active = true;
    particle->position = position;
    particle->velocity = velocity;
    particle->color = color;
    particle->lifetime = lifetime;
    particle->max_lifetime = lifetime;
    particle->size = start_size;
    particle->start_size = start_size;
    particle->end_size = end_size;
    particle->effect_type = effect_type;
}

void ResetParticles()
{
    for (std::size_t i = 0; i < g_Particles.size(); ++i)
        g_Particles[i].active = false;

    for (std::size_t i = 0; i < g_SmokeEffects.size(); ++i)
        g_SmokeEffects[i].active = false;

    for (std::size_t i = 0; i < g_ImpactEffects.size(); ++i)
        g_ImpactEffects[i].active = false;
}

void SpawnSmokeBurst(const glm::vec4 &position)
{
    AnimatedSmokeEffect *smoke = GetInactiveSmokeEffect();
    if (smoke == NULL)
        return;

    smoke->active = true;
    smoke->position = position + glm::vec4(0.0f, 0.10f, 0.0f, 0.0f);
    smoke->lifetime = SMOKE_LIFETIME;
    smoke->max_lifetime = SMOKE_LIFETIME;
    smoke->size = SMOKE_SIZE;
    smoke->color = SMOKE_COLOR_A;
}

void SpawnSlingshotImpact(const glm::vec4 &position)
{
    AnimatedImpactEffect *impact = GetInactiveImpactEffect();
    if (impact == NULL)
        return;

    impact->active = true;
    impact->position = position;
    impact->lifetime = SLINGSHOT_IMPACT_LIFETIME;
    impact->max_lifetime = SLINGSHOT_IMPACT_LIFETIME;
    impact->size = SLINGSHOT_IMPACT_SIZE;
    impact->color = IMPACT_COLOR_A;
}

void UpdateParticles(float delta_time)
{
    for (std::size_t i = 0; i < g_Particles.size(); ++i)
    {
        Particle &particle = g_Particles[i];
        if (!particle.active)
            continue;

        particle.lifetime -= delta_time;
        if (particle.lifetime <= 0.0f)
        {
            particle.active = false;
            continue;
        }

        const float age = 1.0f - particle.lifetime / particle.max_lifetime;
        particle.position += particle.velocity * delta_time;
        particle.velocity.x *= PARTICLE_DRAG;
        particle.velocity.z *= PARTICLE_DRAG;
        particle.velocity.y *= 0.96f;
        particle.size = Lerp(particle.start_size, particle.end_size, age);
        particle.color.a = std::max(0.0f, particle.color.a * (1.0f - age * 0.45f));
    }

    for (std::size_t i = 0; i < g_SmokeEffects.size(); ++i)
    {
        AnimatedSmokeEffect &smoke = g_SmokeEffects[i];
        if (!smoke.active)
            continue;

        smoke.lifetime -= delta_time;
        if (smoke.lifetime <= 0.0f)
        {
            smoke.active = false;
            continue;
        }

        const float age = 1.0f - smoke.lifetime / smoke.max_lifetime;
        smoke.position.y += delta_time * 0.18f;
        smoke.size = SMOKE_SIZE * (1.0f + age * 0.35f);
        smoke.color.a = std::max(0.0f, SMOKE_COLOR_A.a * (1.0f - age));
    }

    for (std::size_t i = 0; i < g_ImpactEffects.size(); ++i)
    {
        AnimatedImpactEffect &impact = g_ImpactEffects[i];
        if (!impact.active)
            continue;

        impact.lifetime -= delta_time;
        if (impact.lifetime <= 0.0f)
        {
            impact.active = false;
            continue;
        }

        const float age = 1.0f - impact.lifetime / impact.max_lifetime;
        impact.color.a = std::max(0.0f, 0.30f * (1.0f - age));
    }
}

static void DrawTexturedQuad(
    const glm::vec4 &position,
    float size,
    const glm::vec4 &color,
    GLuint texture_id,
    GLuint sampler_id,
    const ParticleDrawContext &context)
{
    const glm::vec4 to_camera = context.camera_position - position;
    const float yaw = std::atan2(to_camera.x, to_camera.z);
    const float planar_distance = std::sqrt(to_camera.x * to_camera.x + to_camera.z * to_camera.z);
    const float pitch = -std::atan2(to_camera.y, std::max(0.001f, planar_distance));

    glm::mat4 model =
        Matrix_Translate(position.x, position.y, position.z) *
        Matrix_Rotate_Y(yaw) *
        Matrix_Rotate_X(pitch) *
        Matrix_Scale(size, size, size);

    glUniformMatrix4fv(context.model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(context.object_id_uniform, context.object_id_effect);
    glUniform1i(context.cube_colliding_uniform, 0);
    glUniform3f(
        context.object_tint_uniform,
        color.r,
        color.g,
        color.b);
    glUniform1f(context.effect_alpha_uniform, color.a);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glBindSampler(0, sampler_id);
    glBindVertexArray(g_ParticleQuadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void DrawParticles(const ParticleDrawContext &context)
{
    EnsureParticleQuad();

    glDisable(GL_CULL_FACE);
    for (std::size_t i = 0; i < g_SmokeEffects.size(); ++i)
    {
        const AnimatedSmokeEffect &smoke = g_SmokeEffects[i];
        if (!smoke.active)
            continue;

        if (context.smoke_textures.texture_ids.empty() || context.smoke_textures.sampler_ids.empty())
            continue;

        const float animation_t = 1.0f - smoke.lifetime / smoke.max_lifetime;
        const std::size_t frame_count =
            std::min(context.smoke_textures.texture_ids.size(), context.smoke_textures.sampler_ids.size());
        const std::size_t frame_index = std::min(
            frame_count - 1,
            static_cast<std::size_t>(animation_t * static_cast<float>(frame_count)));

        DrawTexturedQuad(
            smoke.position,
            smoke.size,
            smoke.color,
            context.smoke_textures.texture_ids[frame_index],
            context.smoke_textures.sampler_ids[frame_index],
            context);
    }

    for (std::size_t i = 0; i < g_ImpactEffects.size(); ++i)
    {
        const AnimatedImpactEffect &impact = g_ImpactEffects[i];
        if (!impact.active)
            continue;

        if (context.spark_textures.texture_ids.empty() || context.spark_textures.sampler_ids.empty())
            continue;

        const float animation_t = 1.0f - impact.lifetime / impact.max_lifetime;
        const std::size_t frame_count =
            std::min(context.spark_textures.texture_ids.size(), context.spark_textures.sampler_ids.size());
        const std::size_t frame_index = std::min(
            frame_count - 1,
            static_cast<std::size_t>(animation_t * static_cast<float>(frame_count)));

        DrawTexturedQuad(
            impact.position,
            impact.size,
            impact.color,
            context.spark_textures.texture_ids[frame_index],
            context.spark_textures.sampler_ids[frame_index],
            context);
    }

    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}
