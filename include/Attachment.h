#ifndef ATTACHMENT_H
#define ATTACHMENT_H

#include <string>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

class AssimpModelLoader;

struct ModelAttachment {
    std::string name;
    std::string boneName;
    std::vector<std::string> objectNames;
    glm::vec3 localPosition = glm::vec3(0.0f);
    glm::vec3 localRotationDeg = glm::vec3(0.0f);
    glm::vec3 localScale = glm::vec3(1.0f);
    float uniformScale = 1.0f;
    bool visible = true;
};

void InitAttachments();
ModelAttachment* CreateAttachment(const std::string& name,
                                  const std::string& boneName,
                                  const std::vector<std::string>& objectNames);
void RemoveAttachment(const std::string& name);
ModelAttachment* GetAttachment(const std::string& name);
std::vector<ModelAttachment*>& GetAllAttachments();

void SetAttachmentBone(ModelAttachment* att, const std::string& boneName);
void SetAttachmentOffset(ModelAttachment* att,
                         const glm::vec3& pos,
                         const glm::vec3& rotDeg,
                         const glm::vec3& scale);
void SetAttachmentUniformScale(ModelAttachment* att, float scale);
void SetAttachmentVisible(ModelAttachment* att, bool visible);

glm::mat4 ComputeAttachmentModelMatrix(const ModelAttachment* att,
                                        const AssimpModelLoader& modelLoader,
                                        const glm::mat4& playerWorldMatrix,
                                        const glm::vec4& modelCenter);

#endif // ATTACHMENT_H
