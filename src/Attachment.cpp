#include "Attachment.h"
#include "AssimpModelLoader.h"
#include "matrices.h"
#include <algorithm>
#include <cstdio>

static std::vector<ModelAttachment*> s_attachments;

void InitAttachments()
{
    for (auto* att : s_attachments)
        delete att;
    s_attachments.clear();
}

ModelAttachment* CreateAttachment(const std::string& name,
                                  const std::string& boneName,
                                  const std::vector<std::string>& objectNames)
{
    RemoveAttachment(name);
    ModelAttachment* att = new ModelAttachment();
    att->name = name;
    att->boneName = boneName;
    att->objectNames = objectNames;
    s_attachments.push_back(att);
    printf("[ATTACHMENT] Created '%s' on bone '%s' (%zu objects)\n",
           name.c_str(), boneName.c_str(), objectNames.size());
    return att;
}

void RemoveAttachment(const std::string& name)
{
    for (size_t i = 0; i < s_attachments.size(); ++i)
    {
        if (s_attachments[i]->name == name)
        {
            printf("[ATTACHMENT] Removed '%s'\n", name.c_str());
            delete s_attachments[i];
            s_attachments.erase(s_attachments.begin() + i);
            return;
        }
    }
}

ModelAttachment* GetAttachment(const std::string& name)
{
    for (auto* att : s_attachments)
    {
        if (att->name == name)
            return att;
    }
    return nullptr;
}

std::vector<ModelAttachment*>& GetAllAttachments()
{
    return s_attachments;
}

void SetAttachmentBone(ModelAttachment* att, const std::string& boneName)
{
    if (att)
        att->boneName = boneName;
}

void SetAttachmentOffset(ModelAttachment* att,
                         const glm::vec3& pos,
                         const glm::vec3& rotDeg,
                         const glm::vec3& scale)
{
    if (!att) return;
    att->localPosition = pos;
    att->localRotationDeg = rotDeg;
    att->localScale = scale;
}

void SetAttachmentUniformScale(ModelAttachment* att, float scale)
{
    if (att)
        att->uniformScale = scale;
}

void SetAttachmentVisible(ModelAttachment* att, bool visible)
{
    if (att)
        att->visible = visible;
}

static glm::mat4 ExtractBoneRotation(const glm::mat4& boneTransform)
{
    glm::vec3 col0(boneTransform[0].x, boneTransform[0].y, boneTransform[0].z);
    glm::vec3 col1(boneTransform[1].x, boneTransform[1].y, boneTransform[1].z);

    col0 = glm::normalize(col0);
    col1 = glm::normalize(col1);
    glm::vec3 col2 = glm::cross(col0, col1);
    col1 = glm::cross(col2, col0);

    glm::mat4 rot(1.0f);
    rot[0] = glm::vec4(col0, 0.0f);
    rot[1] = glm::vec4(col1, 0.0f);
    rot[2] = glm::vec4(col2, 0.0f);
    return rot;
}

glm::mat4 ComputeAttachmentModelMatrix(const ModelAttachment* att,
                                        const AssimpModelLoader& modelLoader,
                                        const glm::mat4& playerWorldMatrix,
                                        const glm::vec4& modelCenter)
{
    if (!att) return glm::mat4(1.0f);

    glm::mat4 boneTransform = modelLoader.GetBoneWorldTransform(att->boneName);

    glm::vec3 bonePos(boneTransform[3].x, boneTransform[3].y, boneTransform[3].z);
    glm::vec3 centeredBonePos = bonePos - glm::vec3(modelCenter);

    glm::mat4 boneRotation = ExtractBoneRotation(boneTransform);

    float px = att->localPosition.x;
    float py = att->localPosition.y;
    float pz = att->localPosition.z;
    float rx = att->localRotationDeg.x * 3.14159265f / 180.0f;
    float ry = att->localRotationDeg.y * 3.14159265f / 180.0f;
    float rz = att->localRotationDeg.z * 3.14159265f / 180.0f;
    float s = att->uniformScale;

    glm::mat4 localOffset = Matrix_Rotate_X(rx)
                           * Matrix_Rotate_Y(ry)
                           * Matrix_Rotate_Z(rz)
                           * Matrix_Scale(s, s, s)
                           * Matrix_Translate(px, py, pz);

    return playerWorldMatrix * Matrix_Translate(centeredBonePos.x, centeredBonePos.y, centeredBonePos.z) * boneRotation * localOffset;
}
