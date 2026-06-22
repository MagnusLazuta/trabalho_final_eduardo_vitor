#version 330 core

// Atributos de vértice recebidos como entrada ("in") pelo Vertex Shader.
layout (location = 0) in vec4 model_coefficients;
layout (location = 1) in vec4 normal_coefficients;
layout (location = 2) in vec2 texture_coefficients;
layout (location = 3) in ivec4 boneIDs;
layout (location = 4) in vec4 weights;

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 model_normal_matrix;

// Matrizes de animação (skinning)
const int MAX_BONES = 100;
uniform mat4 bones[MAX_BONES];
uniform int useAnimation;
uniform vec3 weaponOffset;  // Offset adicional para armas (bone-skinned)
uniform int isWeapon;       // 1 se é uma arma, 0 caso contrário

// Atributos de vértice que serão gerados como saída ("out") pelo Vertex Shader.
out vec4 position_world;
out vec4 position_model;
out vec4 normal;
out vec2 texcoords;

void main()
{
    vec4 skinnedPosition = model_coefficients;
    vec4 skinnedNormal = normal_coefficients;
    
    // Aplica deformação de pele (skinning) se houver animação
    if (useAnimation == 1) {
        float totalWeight = weights[0] + weights[1] + weights[2] + weights[3];
        
        if (totalWeight > 0.001) {
            if (isWeapon == 1) {
                // Para armas: vértices já estão em model space na posição do osso.
                // Não aplicamos boneTransform (ele expecta bind pose space).
                // Aplicamos offset adicional para posicionamento manual.
                skinnedPosition = model_coefficients + vec4(weaponOffset, 0.0);
                // Offset é aplicado depois via model matrix (weaponOffset na C++)
            } else {
                mat4 boneTransform = mat4(0.0);
                boneTransform += bones[boneIDs[0]] * weights[0];
                boneTransform += bones[boneIDs[1]] * weights[1];
                boneTransform += bones[boneIDs[2]] * weights[2];
                boneTransform += bones[boneIDs[3]] * weights[3];
                
                skinnedPosition = boneTransform * model_coefficients;
                
                mat3 normalMatrix = transpose(inverse(mat3(boneTransform)));
                skinnedNormal = vec4(normalMatrix * normal_coefficients.xyz, 0.0);
            }
        }
        // Se totalWeight == 0, mantém posição original (sem deformação)
    }
    
    // Posição final do vértice em NDC
    gl_Position = projection * view * model * skinnedPosition;

    // Posição no sistema de coordenadas global (World)
    position_world = model * skinnedPosition;

    // Posição no sistema de coordenadas local do modelo
    position_model = skinnedPosition;

    // Normal no sistema de coordenadas global (World)
    normal = model_normal_matrix * skinnedNormal;
    normal.w = 0.0;

    // Coordenadas de textura
    texcoords = texture_coefficients;
}