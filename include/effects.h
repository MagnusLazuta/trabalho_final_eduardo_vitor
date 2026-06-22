#ifndef _EFFECTS_H
#define _EFFECTS_H

#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/vec4.hpp>

struct Particle
{
    bool active;
    glm::vec4 position;
    glm::vec4 velocity;
    glm::vec4 color;
    float lifetime;
    float max_lifetime;
    float size;
    float start_size;
    float end_size;
    int effect_type;
};

struct ParticleTextureInfo
{
    std::vector<GLuint> texture_ids;
    std::vector<GLuint> sampler_ids;
};

struct ParticleDrawContext
{
    GLint model_uniform;
    GLint object_id_uniform;
    GLint cube_colliding_uniform;
    GLint object_tint_uniform;
    GLint effect_alpha_uniform;
    int object_id_effect;
    glm::vec4 camera_position;
    ParticleTextureInfo smoke_textures;
    ParticleTextureInfo spark_textures;
};

void ResetParticles();
void SpawnSmokeBurst(const glm::vec4 &position);
void SpawnSlingshotImpact(const glm::vec4 &position);
void UpdateParticles(float delta_time);
void DrawParticles(const ParticleDrawContext &context);

#endif // _EFFECTS_H
