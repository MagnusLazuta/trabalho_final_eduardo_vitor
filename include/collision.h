#ifndef _COLLISION_H
#define _COLLISION_H

#include <glm/vec4.hpp>
#include <vector>
#include "matrices.h"
#include "types.h"

struct CollisionRay
{
    glm::vec4 origin;
    glm::vec4 direction;
};

struct CollisionTriangle
{
    glm::vec4 v0;
    glm::vec4 v1;
    glm::vec4 v2;
};

struct CollisionAABB
{
    glm::vec4 min;
    glm::vec4 max;
};

struct CollisionOBB
{
    glm::vec4 center;
    glm::vec4 half_extents;
    float yaw;
};

enum class CollisionShapeType
{
    SOLID,
    DOOR,
    NONE,
    WATER,
    LADDER,
    VINES,
};

struct CollisionShape
{
    CollisionShapeType type;
    glm::vec4 bbox_min;
    glm::vec4 bbox_max;
    std::vector<Triangle> triangles;
};

// Broad Phase: AABB vs AABB
bool AabbAabbIntersect(CollisionAABB a, CollisionAABB b);

// Broad Phase: Ray vs AABB
bool RayAabbIntersect(CollisionRay r, CollisionAABB a, float &t);

// Narrow Phase: Ray vs Triangle
bool RayTriangleIntersect(CollisionRay r, CollisionTriangle tri, float &t);

CollisionShapeType CollidesWithScenarioAabb(const glm::vec4 &center, const glm::vec4 &half_extents, const std::vector<CollisionShape> &g_ScenarioCollisionShapes);

CollisionShapeType CollidesWithScenario(const glm::vec4 &cube_center, const std::vector<CollisionShape> &g_ScenarioCollisionShapes, const glm::vec4 &g_PlayerCubeHalfExtents);

CollisionShapeType CollidesWithScenarioObb(const CollisionOBB &obb, const std::vector<CollisionShape> &g_ScenarioCollisionShapes);

bool IsCollidingWithType(const glm::vec4 &center, const glm::vec4 &half_extents, const std::vector<CollisionShape> &g_ScenarioCollisionShapes, CollisionShapeType type);

bool IsCollidingWithTypeObb(const CollisionOBB &obb, const std::vector<CollisionShape> &g_ScenarioCollisionShapes, CollisionShapeType type);

bool GetVinesNormal(const glm::vec4 &center, const glm::vec4 &half_extents, const std::vector<CollisionShape> &g_ScenarioCollisionShapes, glm::vec4 &out_normal);

bool TriangleIntersectsAabb(const Triangle &triangle, const glm::vec4 &box_center, const glm::vec4 &box_half_extents);

bool TriangleIntersectsObb(const Triangle &triangle, const CollisionOBB &obb);

static bool OverlapOnAxis(
    const glm::vec4 &v0,
    const glm::vec4 &v1,
    const glm::vec4 &v2,
    const glm::vec4 &axis,
    const glm::vec4 &half_extents);

#endif // _COLLISION_H
