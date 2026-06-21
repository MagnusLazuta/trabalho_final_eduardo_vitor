#include "AssimpModelLoader.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

AssimpModelLoader::AssimpModelLoader() 
    : m_numBones(0), m_currentAnimation(0), m_scene(nullptr) {
}

AssimpModelLoader::~AssimpModelLoader() {
}

bool AssimpModelLoader::LoadModel(const std::string& filePath) {
    m_scene = m_importer.ReadFile(filePath, 
        aiProcess_Triangulate | 
        aiProcess_FlipUVs | 
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes |
        aiProcess_LimitBoneWeights);

    if (!m_scene || m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !m_scene->mRootNode) {
        std::cerr << "ASSIMP ERROR: " << m_importer.GetErrorString() << std::endl;
        return false;
    }

    // Converte aiMatrix4x4 (row-major) para glm::mat4 (column-major) e calcula inversa
    glm::mat4 rootTransform(
        m_scene->mRootNode->mTransformation.a1, m_scene->mRootNode->mTransformation.b1, m_scene->mRootNode->mTransformation.c1, m_scene->mRootNode->mTransformation.d1,
        m_scene->mRootNode->mTransformation.a2, m_scene->mRootNode->mTransformation.b2, m_scene->mRootNode->mTransformation.c2, m_scene->mRootNode->mTransformation.d2,
        m_scene->mRootNode->mTransformation.a3, m_scene->mRootNode->mTransformation.b3, m_scene->mRootNode->mTransformation.c3, m_scene->mRootNode->mTransformation.d3,
        m_scene->mRootNode->mTransformation.a4, m_scene->mRootNode->mTransformation.b4, m_scene->mRootNode->mTransformation.c4, m_scene->mRootNode->mTransformation.d4
    );
    m_globalInverseTransform = glm::inverse(rootTransform);
    
    m_meshes.clear();
    m_materials.clear();
    m_animations.clear();
    m_boneInfoMap.clear();
    m_boneMapping.clear();
    m_numBones = 0;
    
    ProcessMaterials(m_scene);
    ProcessNode(m_scene->mRootNode, m_scene);
    ProcessAnimations(m_scene);
    
    printf("=== FBX Loading Report ===\n");
    printf("Meshes: %zu\n", m_meshes.size());
    printf("Materials: %zu\n", m_materials.size());
    printf("Animations: %zu\n", m_animations.size());
    printf("Bones: %u\n", m_numBones);
    
    // Print root node transform
    printf("Root node transform:\n");
    for (int r = 0; r < 4; r++) {
        printf("  [%.4f, %.4f, %.4f, %.4f]\n",
               m_scene->mRootNode->mTransformation[r][0],
               m_scene->mRootNode->mTransformation[r][1],
               m_scene->mRootNode->mTransformation[r][2],
               m_scene->mRootNode->mTransformation[r][3]);
    }
    printf("Global inverse transform:\n");
    for (int r = 0; r < 4; r++) {
        printf("  [%.4f, %.4f, %.4f, %.4f]\n",
               m_globalInverseTransform[r][0],
               m_globalInverseTransform[r][1],
               m_globalInverseTransform[r][2],
               m_globalInverseTransform[r][3]);
    }
    
    if (!m_animations.empty()) {
        printf("Animation: name='%s', duration=%.2f, tps=%.2f, channels=%zu\n",
               m_animations[0].name.c_str(), m_animations[0].duration,
               m_animations[0].ticksPerSecond, m_animations[0].channels.size());
        
        // Print all channel names with key counts
        printf("Channel names:\n");
        for (size_t i = 0; i < m_animations[0].channels.size(); i++) {
            printf("  '%s' posKeys=%zu rotKeys=%zu scaleKeys=%zu\n",
                   m_animations[0].channels[i].nodeName.c_str(),
                   m_animations[0].channels[i].positions.size(),
                   m_animations[0].channels[i].rotations.size(),
                   m_animations[0].channels[i].scales.size());
        }
    }
    
    // Print first bone offset
    if (!m_boneInfoMap.empty()) {
        auto it = m_boneInfoMap.begin();
        printf("First bone '%s' offset:\n", it->first.c_str());
        for (int r = 0; r < 4; r++) {
            printf("  [%.4f, %.4f, %.4f, %.4f]\n",
                   it->second.offset[r][0], it->second.offset[r][1],
                   it->second.offset[r][2], it->second.offset[r][3]);
        }
    }
    
    // Print vertex bounds
    float min_x = 1e10, max_x = -1e10;
    float min_y = 1e10, max_y = -1e10;
    float min_z = 1e10, max_z = -1e10;
    for (const auto& mesh : m_meshes) {
        for (size_t i = 0; i < mesh.vertices.size(); i += 3) {
            float x = mesh.vertices[i];
            float y = mesh.vertices[i+1];
            float z = mesh.vertices[i+2];
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
            if (z < min_z) min_z = z;
            if (z > max_z) max_z = z;
        }
    }
    printf("Vertex bounds: X[%.2f, %.2f] Y[%.2f, %.2f] Z[%.2f, %.2f]\n",
           min_x, max_x, min_y, max_y, min_z, max_z);
    printf("Model size: (%.2f, %.2f, %.2f)\n", max_x-min_x, max_y-min_y, max_z-min_z);
    printf("===========================\n");
    
    return true;
}

bool AssimpModelLoader::LoadAnimation(const std::string& filePath) {
    printf("Attempting to load animation from: '%s'\n", filePath.c_str());
    Assimp::Importer animImporter;
    
    // Tenta carregar com flags mínimas (pode ser arquivo de animação pura)
    const aiScene* animScene = animImporter.ReadFile(filePath, 0);

    if (!animScene || !animScene->mRootNode) {
        std::cerr << "ASSIMP ERROR loading animation: " << animImporter.GetErrorString() << std::endl;
        return false;
    }
    
    if (!animScene->HasAnimations()) {
        printf("No animations found in %s\n", filePath.c_str());
        return false;
    }

    // Clear existing animations and load new ones
    m_animations.clear();

    for (unsigned int i = 0; i < animScene->mNumAnimations; i++) {
        aiAnimation* animation = animScene->mAnimations[i];
        AssimpAnimation anim;
        
        anim.name = animation->mName.length > 0 ? animation->mName.C_Str() : "animation_" + std::to_string(i);
        anim.duration = animation->mDuration;
        anim.ticksPerSecond = animation->mTicksPerSecond > 0 ? animation->mTicksPerSecond : 30.0f;
        
        printf("Loading animation: name='%s', duration=%.2f, tps=%.2f, channels=%u\n",
               anim.name.c_str(), anim.duration, anim.ticksPerSecond, animation->mNumChannels);
        
        for (unsigned int j = 0; j < animation->mNumChannels; j++) {
            aiNodeAnim* channel = animation->mChannels[j];
            AssimpAnimation::Channel animChannel;
            
            animChannel.nodeName = channel->mNodeName.C_Str();
            
            for (unsigned int k = 0; k < channel->mNumPositionKeys; k++) {
                animChannel.positionKeys.push_back(channel->mPositionKeys[k].mTime);
                animChannel.positions.push_back(glm::vec3(
                    channel->mPositionKeys[k].mValue.x,
                    channel->mPositionKeys[k].mValue.y,
                    channel->mPositionKeys[k].mValue.z
                ));
            }
            
            for (unsigned int k = 0; k < channel->mNumRotationKeys; k++) {
                animChannel.rotationKeys.push_back(channel->mRotationKeys[k].mTime);
                animChannel.rotations.push_back(glm::quat(
                    channel->mRotationKeys[k].mValue.w,
                    channel->mRotationKeys[k].mValue.x,
                    channel->mRotationKeys[k].mValue.y,
                    channel->mRotationKeys[k].mValue.z
                ));
            }
            
            for (unsigned int k = 0; k < channel->mNumScalingKeys; k++) {
                animChannel.scaleKeys.push_back(channel->mScalingKeys[k].mTime);
                animChannel.scales.push_back(glm::vec3(
                    channel->mScalingKeys[k].mValue.x,
                    channel->mScalingKeys[k].mValue.y,
                    channel->mScalingKeys[k].mValue.z
                ));
            }
            
            anim.channels.push_back(animChannel);
        }
        
        m_animations.push_back(anim);
        // Only keep the first animation for now
        
        // Pre-build channel lookup
        m_channelLookup.resize(m_animations.size());
        m_channelLookup[m_animations.size() - 1].clear();
        m_channelLookup[m_animations.size() - 1].reserve(anim.channels.size());
        for (size_t c = 0; c < anim.channels.size(); ++c) {
            m_channelLookup[m_animations.size() - 1][anim.channels[c].nodeName] = c;
        }
        break;
    }
    
    printf("Loaded %zu animations from %s\n", m_animations.size(), filePath.c_str());
    return !m_animations.empty();
}

bool AssimpModelLoader::AddAnimation(const std::string& filePath) {
    printf("Attempting to add animation from: '%s'\n", filePath.c_str());
    Assimp::Importer animImporter;
    
    const aiScene* animScene = animImporter.ReadFile(filePath, 0);

    if (!animScene || !animScene->mRootNode) {
        std::cerr << "ASSIMP ERROR loading animation: " << animImporter.GetErrorString() << std::endl;
        return false;
    }
    
    if (!animScene->HasAnimations()) {
        printf("No animations found in %s\n", filePath.c_str());
        return false;
    }

    for (unsigned int i = 0; i < animScene->mNumAnimations; i++) {
        aiAnimation* animation = animScene->mAnimations[i];
        AssimpAnimation anim;
        
        anim.name = animation->mName.length > 0 ? animation->mName.C_Str() : "animation_" + std::to_string(i);
        anim.duration = animation->mDuration;
        anim.ticksPerSecond = animation->mTicksPerSecond > 0 ? animation->mTicksPerSecond : 30.0f;
        
        printf("Adding animation: name='%s', duration=%.2f, tps=%.2f, channels=%u\n",
               anim.name.c_str(), anim.duration, anim.ticksPerSecond, animation->mNumChannels);
        
        for (unsigned int j = 0; j < animation->mNumChannels; j++) {
            aiNodeAnim* channel = animation->mChannels[j];
            AssimpAnimation::Channel animChannel;
            
            animChannel.nodeName = channel->mNodeName.C_Str();
            
            for (unsigned int k = 0; k < channel->mNumPositionKeys; k++) {
                animChannel.positionKeys.push_back(channel->mPositionKeys[k].mTime);
                animChannel.positions.push_back(glm::vec3(
                    channel->mPositionKeys[k].mValue.x,
                    channel->mPositionKeys[k].mValue.y,
                    channel->mPositionKeys[k].mValue.z
                ));
            }
            
            for (unsigned int k = 0; k < channel->mNumRotationKeys; k++) {
                animChannel.rotationKeys.push_back(channel->mRotationKeys[k].mTime);
                animChannel.rotations.push_back(glm::quat(
                    channel->mRotationKeys[k].mValue.w,
                    channel->mRotationKeys[k].mValue.x,
                    channel->mRotationKeys[k].mValue.y,
                    channel->mRotationKeys[k].mValue.z
                ));
            }
            
            for (unsigned int k = 0; k < channel->mNumScalingKeys; k++) {
                animChannel.scaleKeys.push_back(channel->mScalingKeys[k].mTime);
                animChannel.scales.push_back(glm::vec3(
                    channel->mScalingKeys[k].mValue.x,
                    channel->mScalingKeys[k].mValue.y,
                    channel->mScalingKeys[k].mValue.z
                ));
            }
            
            anim.channels.push_back(animChannel);
        }
        
        m_animations.push_back(anim);
        
        // Pre-build channel lookup
        m_channelLookup.resize(m_animations.size());
        m_channelLookup[m_animations.size() - 1].clear();
        m_channelLookup[m_animations.size() - 1].reserve(anim.channels.size());
        for (size_t c = 0; c < anim.channels.size(); ++c) {
            m_channelLookup[m_animations.size() - 1][anim.channels[c].nodeName] = c;
        }
        break; // Only load first animation per file
    }
    
    printf("Total animations now: %zu\n", m_animations.size());
    return true;
}

void AssimpModelLoader::SetCurrentAnimation(int index) {
    if (index >= 0 && index < (int)m_animations.size()) {
        m_currentAnimation = index;
    }
}

void AssimpModelLoader::ProcessNode(aiNode* node, const aiScene* scene) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene);
    }
    
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene);
    }
}

void AssimpModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene) {
    AssimpMeshData meshData;
    
    meshData.name = mesh->mName.length > 0 ? mesh->mName.C_Str() : "mesh_" + std::to_string(m_meshes.size());
    meshData.materialIndex = mesh->mMaterialIndex;
    
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        meshData.vertices.push_back(mesh->mVertices[i].x);
        meshData.vertices.push_back(mesh->mVertices[i].y);
        meshData.vertices.push_back(mesh->mVertices[i].z);
        
        if (mesh->HasNormals()) {
            meshData.normals.push_back(mesh->mNormals[i].x);
            meshData.normals.push_back(mesh->mNormals[i].y);
            meshData.normals.push_back(mesh->mNormals[i].z);
        }
        
        if (mesh->mTextureCoords[0]) {
            meshData.texcoords.push_back(mesh->mTextureCoords[0][i].x);
            meshData.texcoords.push_back(mesh->mTextureCoords[0][i].y);
        } else {
            meshData.texcoords.push_back(0.0f);
            meshData.texcoords.push_back(0.0f);
        }
    }
    
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            meshData.indices.push_back(face.mIndices[j]);
        }
    }
    
    if (mesh->HasBones()) {
        meshData.boneData.resize(mesh->mNumVertices);
        ExtractBoneWeightForVertices(meshData, mesh, scene);
    }
    
    m_meshes.push_back(meshData);
}

void AssimpModelLoader::ExtractBoneWeightForVertices(AssimpMeshData& mesh, aiMesh* aiMesh, const aiScene* scene) {
    for (unsigned int i = 0; i < aiMesh->mNumBones; i++) {
        unsigned int boneIndex = 0;
        std::string boneName(aiMesh->mBones[i]->mName.C_Str());
        
        if (m_boneMapping.find(boneName) == m_boneMapping.end()) {
            boneIndex = m_numBones;
            m_numBones++;
            AssimpBoneInfo bi;
            // Converte aiMatrix4x4 (row-major) para glm::mat4 (column-major)
            bi.offset = glm::mat4(
                aiMesh->mBones[i]->mOffsetMatrix.a1, aiMesh->mBones[i]->mOffsetMatrix.b1, aiMesh->mBones[i]->mOffsetMatrix.c1, aiMesh->mBones[i]->mOffsetMatrix.d1,
                aiMesh->mBones[i]->mOffsetMatrix.a2, aiMesh->mBones[i]->mOffsetMatrix.b2, aiMesh->mBones[i]->mOffsetMatrix.c2, aiMesh->mBones[i]->mOffsetMatrix.d2,
                aiMesh->mBones[i]->mOffsetMatrix.a3, aiMesh->mBones[i]->mOffsetMatrix.b3, aiMesh->mBones[i]->mOffsetMatrix.c3, aiMesh->mBones[i]->mOffsetMatrix.d3,
                aiMesh->mBones[i]->mOffsetMatrix.a4, aiMesh->mBones[i]->mOffsetMatrix.b4, aiMesh->mBones[i]->mOffsetMatrix.c4, aiMesh->mBones[i]->mOffsetMatrix.d4
            );
            bi.index = boneIndex;
            m_boneInfoMap[boneName] = bi;
            m_boneMapping[boneName] = boneIndex;
        } else {
            boneIndex = m_boneMapping[boneName];
        }
        
        for (unsigned int j = 0; j < aiMesh->mBones[i]->mNumWeights; j++) {
            unsigned int vertexIndex = aiMesh->mBones[i]->mWeights[j].mVertexId;
            float weight = aiMesh->mBones[i]->mWeights[j].mWeight;
            
            for (unsigned int k = 0; k < 4; k++) {
                if (mesh.boneData[vertexIndex].weights[k] == 0.0) {
                    mesh.boneData[vertexIndex].boneIDs[k] = boneIndex;
                    mesh.boneData[vertexIndex].weights[k] = weight;
                    break;
                }
            }
        }
    }
}

void AssimpModelLoader::ProcessMaterials(const aiScene* scene) {
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* material = scene->mMaterials[i];
        AssimpMaterial mat;
        
        aiString name;
        if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
            mat.name = name.C_Str();
        }
        
        aiColor3D diffuse(0.8f, 0.8f, 0.8f);
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
            mat.diffuse = glm::vec3(diffuse.r, diffuse.g, diffuse.b);
        }
        
        aiColor3D specular(0.0f, 0.0f, 0.0f);
        if (material->Get(AI_MATKEY_COLOR_SPECULAR, specular) == AI_SUCCESS) {
            mat.specular = glm::vec3(specular.r, specular.g, specular.b);
        }
        
        aiColor3D ambient(0.2f, 0.2f, 0.2f);
        if (material->Get(AI_MATKEY_COLOR_AMBIENT, ambient) == AI_SUCCESS) {
            mat.ambient = glm::vec3(ambient.r, ambient.g, ambient.b);
        }
        
        float shininess = 0.0f;
        if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
            mat.shininess = shininess;
        }
        
        aiString texPath;
        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
            mat.diffuseTexture = texPath.C_Str();
        }
        
        if (material->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS) {
            mat.normalTexture = texPath.C_Str();
        }
        
        if (material->GetTexture(aiTextureType_SPECULAR, 0, &texPath) == AI_SUCCESS) {
            mat.specularTexture = texPath.C_Str();
        }
        
        m_materials.push_back(mat);
    }
}

void AssimpModelLoader::ProcessAnimations(const aiScene* scene) {
    if (!scene->HasAnimations()) {
        return;
    }
    
    for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
        aiAnimation* animation = scene->mAnimations[i];
        AssimpAnimation anim;
        
        anim.name = animation->mName.length > 0 ? animation->mName.C_Str() : "animation_" + std::to_string(i);
        anim.duration = animation->mDuration;
        anim.ticksPerSecond = animation->mTicksPerSecond > 0 ? animation->mTicksPerSecond : 25.0f;
        
        for (unsigned int j = 0; j < animation->mNumChannels; j++) {
            aiNodeAnim* channel = animation->mChannels[j];
            AssimpAnimation::Channel animChannel;
            
            animChannel.nodeName = channel->mNodeName.C_Str();
            
            for (unsigned int k = 0; k < channel->mNumPositionKeys; k++) {
                animChannel.positionKeys.push_back(channel->mPositionKeys[k].mTime);
                animChannel.positions.push_back(glm::vec3(
                    channel->mPositionKeys[k].mValue.x,
                    channel->mPositionKeys[k].mValue.y,
                    channel->mPositionKeys[k].mValue.z
                ));
            }
            
            for (unsigned int k = 0; k < channel->mNumRotationKeys; k++) {
                animChannel.rotationKeys.push_back(channel->mRotationKeys[k].mTime);
                animChannel.rotations.push_back(glm::quat(
                    channel->mRotationKeys[k].mValue.w,
                    channel->mRotationKeys[k].mValue.x,
                    channel->mRotationKeys[k].mValue.y,
                    channel->mRotationKeys[k].mValue.z
                ));
            }
            
            for (unsigned int k = 0; k < channel->mNumScalingKeys; k++) {
                animChannel.scaleKeys.push_back(channel->mScalingKeys[k].mTime);
                animChannel.scales.push_back(glm::vec3(
                    channel->mScalingKeys[k].mValue.x,
                    channel->mScalingKeys[k].mValue.y,
                    channel->mScalingKeys[k].mValue.z
                ));
            }
            
            anim.channels.push_back(animChannel);
        }
        
        m_animations.push_back(anim);
        
        // Pre-build channel lookup for fast name → index mapping
        int animIdx = (int)m_animations.size() - 1;
        m_channelLookup.resize(m_animations.size());
        m_channelLookup[animIdx].clear();
        m_channelLookup[animIdx].reserve(anim.channels.size());
        for (size_t c = 0; c < anim.channels.size(); ++c) {
            m_channelLookup[animIdx][anim.channels[c].nodeName] = c;
        }
    }
}

template<typename T>
static unsigned int FindKeyBinary(float animationTime, const std::vector<T>& keys) {
    if (keys.size() <= 1) return 0;
    auto it = std::upper_bound(keys.begin(), keys.end(), animationTime);
    if (it == keys.begin()) return 0;
    return (unsigned int)(it - keys.begin() - 1);
}

unsigned int AssimpModelLoader::FindPositionKey(float animationTime, const AssimpAnimation::Channel& channel) {
    return FindKeyBinary(animationTime, channel.positionKeys);
}

unsigned int AssimpModelLoader::FindRotationKey(float animationTime, const AssimpAnimation::Channel& channel) {
    return FindKeyBinary(animationTime, channel.rotationKeys);
}

unsigned int AssimpModelLoader::FindScalingKey(float animationTime, const AssimpAnimation::Channel& channel) {
    return FindKeyBinary(animationTime, channel.scaleKeys);
}

void AssimpModelLoader::ReadNodeHierarchy(float animationTime, aiNode* node, const glm::mat4& parentTransform, int depth) {
    const char* nodeNameCStr = node->mName.C_Str();
    
    // Converte aiMatrix4x4 (row-major) para glm::mat4 (column-major)
    glm::mat4 nodeTransformation(
        node->mTransformation.a1, node->mTransformation.b1, node->mTransformation.c1, node->mTransformation.d1,
        node->mTransformation.a2, node->mTransformation.b2, node->mTransformation.c2, node->mTransformation.d2,
        node->mTransformation.a3, node->mTransformation.b3, node->mTransformation.c3, node->mTransformation.d3,
        node->mTransformation.a4, node->mTransformation.b4, node->mTransformation.c4, node->mTransformation.d4
    );
    
    if (m_currentAnimation >= 0 && m_currentAnimation < (int)m_animations.size() 
        && m_currentAnimation < (int)m_channelLookup.size()) {
        const AssimpAnimation& animation = m_animations[m_currentAnimation];
        const auto& lookup = m_channelLookup[m_currentAnimation];
        
        auto channelIt = lookup.find(nodeNameCStr);
        if (channelIt != lookup.end()) {
            const AssimpAnimation::Channel& channel = animation.channels[channelIt->second];
            
            glm::vec3 position = glm::vec3(nodeTransformation[3]);
            glm::quat rotation = glm::toQuat(nodeTransformation);
            glm::vec3 scale = glm::vec3(
                glm::length(glm::vec3(nodeTransformation[0])),
                glm::length(glm::vec3(nodeTransformation[1])),
                glm::length(glm::vec3(nodeTransformation[2]))
            );
            
            if (!channel.positions.empty() && channel.positions.size() >= 2) {
                unsigned int posIndex = FindPositionKey(animationTime, channel);
                if (posIndex + 1 < channel.positions.size() && posIndex + 1 < channel.positionKeys.size()) {
                    float delta = channel.positionKeys[posIndex + 1] - channel.positionKeys[posIndex];
                    float t = (delta > 1e-6f) ? (animationTime - channel.positionKeys[posIndex]) / delta : 0.0f;
                    t = glm::clamp(t, 0.0f, 1.0f);
                    position = glm::mix(channel.positions[posIndex], channel.positions[posIndex + 1], t);
                } else {
                    position = channel.positions[posIndex];
                }
            } else if (!channel.positions.empty()) {
                position = channel.positions[0];
            }
            
            if (!channel.rotations.empty() && channel.rotations.size() >= 2) {
                unsigned int rotIndex = FindRotationKey(animationTime, channel);
                if (rotIndex + 1 < channel.rotations.size() && rotIndex + 1 < channel.rotationKeys.size()) {
                    float delta = channel.rotationKeys[rotIndex + 1] - channel.rotationKeys[rotIndex];
                    float t = (delta > 1e-6f) ? (animationTime - channel.rotationKeys[rotIndex]) / delta : 0.0f;
                    t = glm::clamp(t, 0.0f, 1.0f);
                    rotation = glm::slerp(channel.rotations[rotIndex], channel.rotations[rotIndex + 1], t);
                } else {
                    rotation = channel.rotations[rotIndex];
                }
            } else if (!channel.rotations.empty()) {
                rotation = channel.rotations[0];
            }
            
            if (!channel.scales.empty() && channel.scales.size() >= 2) {
                unsigned int scaleIndex = FindScalingKey(animationTime, channel);
                if (scaleIndex + 1 < channel.scales.size() && scaleIndex + 1 < channel.scaleKeys.size()) {
                    float delta = channel.scaleKeys[scaleIndex + 1] - channel.scaleKeys[scaleIndex];
                    float t = (delta > 1e-6f) ? (animationTime - channel.scaleKeys[scaleIndex]) / delta : 0.0f;
                    t = glm::clamp(t, 0.0f, 1.0f);
                    scale = glm::mix(channel.scales[scaleIndex], channel.scales[scaleIndex + 1], t);
                } else {
                    scale = channel.scales[scaleIndex];
                }
            } else if (!channel.scales.empty()) {
                scale = channel.scales[0];
            }
            
            glm::mat4 T = glm::mat4(1.0f);
            T[3] = glm::vec4(position.x, position.y, position.z, 1.0f);
            glm::mat4 R = glm::mat4_cast(rotation);
            glm::mat4 Sc = glm::mat4(1.0f);
            Sc[0][0] = scale.x;
            Sc[1][1] = scale.y;
            Sc[2][2] = scale.z;
            nodeTransformation = T * R * Sc;
        }
    }
    
    glm::mat4 globalTransformation = parentTransform * nodeTransformation;
    
    auto boneIt = m_boneInfoMap.find(nodeNameCStr);
    if (boneIt != m_boneInfoMap.end()) {
        boneIt->second.finalTransformation = m_globalInverseTransform * globalTransformation * boneIt->second.offset;
        m_boneWorldPositions[nodeNameCStr] = glm::vec3(globalTransformation[3]);
        if (strcmp(nodeNameCStr, "mixamorig:Hips") == 0) {
            m_hipsWorldPosition = glm::vec4(globalTransformation[3].x, globalTransformation[3].y, globalTransformation[3].z, 1.0f);
        }
    }
    
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ReadNodeHierarchy(animationTime, node->mChildren[i], globalTransformation, depth + 1);
    }
}

void AssimpModelLoader::GetBoneTransforms(float timeInSeconds, std::vector<glm::mat4>& transforms) {
    if (m_animations.empty() || !m_scene) {
        return;
    }
    
    if (m_currentAnimation < 0 || m_currentAnimation >= (int)m_animations.size()) {
        m_currentAnimation = 0;
    }
    
    const AssimpAnimation& anim = m_animations[m_currentAnimation];
    float timeInTicks = timeInSeconds * anim.ticksPerSecond;
    float animationTime = fmod(timeInTicks, anim.duration > 0.0f ? anim.duration : 1.0f);
    
    static int boneDebugFrameCounter = 0;
    boneDebugFrameCounter++;
    if (boneDebugFrameCounter % 120 == 0) {
        printf("[BONE DEBUG] anim='%s' idx=%d timeSec=%.4f tps=%.2f dur=%.2f timeTicks=%.2f animTick=%.2f chanCount=%zu\n",
               anim.name.c_str(), m_currentAnimation, timeInSeconds, anim.ticksPerSecond,
               anim.duration, timeInTicks, animationTime, anim.channels.size());
    }
    
    ReadNodeHierarchy(animationTime, m_scene->mRootNode, glm::mat4(1.0f));
    
    transforms.resize(m_numBones);
    for (auto& pair : m_boneInfoMap) {
        transforms[pair.second.index] = pair.second.finalTransformation;
    }
}

void AssimpModelLoader::GetFinalBoneTransforms(std::vector<glm::mat4>& transforms) {
    transforms.resize(m_numBones);
    unsigned int index = 0;
    for (auto& pair : m_boneInfoMap) {
        transforms[index++] = pair.second.finalTransformation;
    }
}

void AssimpModelLoader::SampleBoneHierarchy(int animIndex, float timeInSeconds) {
    if (!m_scene || m_animations.empty()) return;
    if (animIndex < 0 || animIndex >= (int)m_animations.size()) return;

    const AssimpAnimation& anim = m_animations[animIndex];
    float tps = anim.ticksPerSecond > 0.0f ? anim.ticksPerSecond : 30.0f;
    float dur = anim.duration > 0.0f ? anim.duration : 1.0f;
    float timeInTicks = timeInSeconds * tps;
    float animationTime = fmod(timeInTicks, dur);

    // Salva/restaura estado da animação corrente para não interferir no game loop
    int savedCurrent = m_currentAnimation;
    m_currentAnimation = animIndex;

    ReadNodeHierarchy(animationTime, m_scene->mRootNode, glm::mat4(1.0f));

    m_currentAnimation = savedCurrent;
}