#ifndef ASSIMP_MODEL_LOADER_H
#define ASSIMP_MODEL_LOADER_H

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

struct AssimpBoneInfo {
    glm::mat4 offset;
    glm::mat4 finalTransformation;
    unsigned int index;
};

struct AssimpVertexBoneData {
    unsigned int boneIDs[4];
    float weights[4];
};

struct AssimpMeshData {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<unsigned int> indices;
    std::vector<AssimpVertexBoneData> boneData;
    std::string name;
    unsigned int materialIndex;
};

struct AssimpMaterial {
    std::string name;
    glm::vec3 diffuse;
    glm::vec3 specular;
    glm::vec3 ambient;
    float shininess;
    std::string diffuseTexture;
    std::string normalTexture;
    std::string specularTexture;
};

struct AssimpAnimation {
    std::string name;
    float duration;
    float ticksPerSecond;
    
    struct Channel {
        std::string nodeName;
        std::vector<float> positionKeys;
        std::vector<float> rotationKeys;
        std::vector<float> scaleKeys;
        std::vector<glm::vec3> positions;
        std::vector<glm::quat> rotations;
        std::vector<glm::vec3> scales;
    };
    std::vector<Channel> channels;
};

class AssimpModelLoader {
public:
    AssimpModelLoader();
    ~AssimpModelLoader();

    bool LoadModel(const std::string& filePath);
    bool LoadAnimation(const std::string& filePath);
    bool AddAnimation(const std::string& filePath);
    void SetCurrentAnimation(int index);
    int GetCurrentAnimation() const { return m_currentAnimation; }
    const std::vector<AssimpMeshData>& GetMeshes() const { return m_meshes; }
    const std::vector<AssimpMaterial>& GetMaterials() const { return m_materials; }
    const std::vector<AssimpAnimation>& GetAnimations() const { return m_animations; }
    const std::map<std::string, AssimpBoneInfo>& GetBoneInfoMap() const { return m_boneInfoMap; }
    unsigned int GetNumBones() const { return m_numBones; }
    
    void ProcessNode(aiNode* node, const aiScene* scene);
    void ProcessMesh(aiMesh* mesh, const aiScene* scene);
    void ProcessMaterials(const aiScene* scene);
    void ProcessAnimations(const aiScene* scene);
    void ReadNodeHierarchy(float animationTime, aiNode* node, const glm::mat4& parentTransform, int depth = 0);
    
    void GetBoneTransforms(float timeInSeconds, std::vector<glm::mat4>& transforms);
    void GetFinalBoneTransforms(std::vector<glm::mat4>& transforms);
    glm::vec4 GetHipsWorldPosition() const { return m_hipsWorldPosition; }
    const glm::vec4& GetHipsWorldPositionRef() const { return m_hipsWorldPosition; }
    glm::mat4 GetBoneWorldTransform(const std::string& boneName) const {
        auto it = m_boneWorldTransforms.find(boneName);
        if (it != m_boneWorldTransforms.end()) return it->second;
        return glm::mat4(1.0f);
    }
    float GetCurrentTicksPerSecond() const {
        if (m_currentAnimation >= 0 && m_currentAnimation < (int)m_animations.size())
            return m_animations[m_currentAnimation].ticksPerSecond;
        return 0.0f;
    }
    float GetCurrentDuration() const {
        if (m_currentAnimation >= 0 && m_currentAnimation < (int)m_animations.size())
            return m_animations[m_currentAnimation].duration;
        return 0.0f;
    }
    
    float GetAnimationDurationSeconds(int animIndex) const {
        if (animIndex >= 0 && animIndex < (int)m_animations.size()) {
            float tps = m_animations[animIndex].ticksPerSecond;
            float dur = m_animations[animIndex].duration;
            return (tps > 0.0f) ? (dur / tps) : 0.0f;
        }
        return 0.0f;
    }
    
    bool HasBone(const std::string& boneName) const {
        return m_boneInfoMap.find(boneName) != m_boneInfoMap.end();
    }
    
    glm::vec3 GetLastSampledBonePos(const std::string& boneName) const {
        auto it = m_boneWorldPositions.find(boneName);
        if (it != m_boneWorldPositions.end()) return it->second;
        return glm::vec3(0.0f);
    }
    
    // Amostra hierarquia de ossos num tempo e animação específicos sem afetar
    // o estado da animação em execução.  Preenche m_boneWorldPositions.
    void SampleBoneHierarchy(int animIndex, float timeInSeconds);
    
    unsigned int FindPositionKey(float animationTime, const AssimpAnimation::Channel& channel);
    unsigned int FindRotationKey(float animationTime, const AssimpAnimation::Channel& channel);
    unsigned int FindScalingKey(float animationTime, const AssimpAnimation::Channel& channel);

private:
    std::vector<AssimpMeshData> m_meshes;
    std::vector<AssimpMaterial> m_materials;
    std::vector<AssimpAnimation> m_animations;
    std::map<std::string, AssimpBoneInfo> m_boneInfoMap;
    std::map<std::string, unsigned int> m_boneMapping;
    std::vector<std::unordered_map<std::string, size_t>> m_channelLookup;
    std::vector<glm::mat4> m_finalBoneTransforms;
    unsigned int m_numBones;
    int m_currentAnimation;
    
    Assimp::Importer m_importer;
    const aiScene* m_scene;
    
    glm::mat4 m_globalInverseTransform;
    glm::vec4 m_hipsWorldPosition;
    
    // Cache de posições world (model-space) dos ossos, preenchido pelo último
    // SampleBoneHierarchy ou GetBoneTransforms (via ReadNodeHierarchy).
    mutable std::unordered_map<std::string, glm::vec3> m_boneWorldPositions;
    mutable std::unordered_map<std::string, glm::mat4> m_boneWorldTransforms;
    
    void ExtractBoneWeightForVertices(AssimpMeshData& mesh, aiMesh* aiMesh, const aiScene* scene);
    void InitBoneTransforms();
};

#endif // ASSIMP_MODEL_LOADER_H