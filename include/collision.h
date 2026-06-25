#ifndef _COLLISION_H
#define _COLLISION_H

#include <glm/vec4.hpp>
#include <vector>
#include <string>
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
    GROUND,
    DOOR,
    NONE,
    WATER,
    GHOST_LADDER,
    LADDER,
    COBWEB_FLOORHOLE,
    VINES,
};

struct CollisionShape
{
    CollisionShapeType type;
    glm::vec4 bbox_min;
    glm::vec4 bbox_max;
    std::vector<Triangle> triangles;
};

enum class DoorState
{
    CLOSED,
    OPENING,
    OPEN,
    CLOSING,
};

struct DoorInstance
{
    DoorState state = DoorState::CLOSED;
    float current_y_offset = 0.0f;
    float target_height = 2.0f;
    float open_timer = 0.0f;
    glm::vec4 bbox_center = glm::vec4(0.0f);
    glm::vec4 bbox_min = glm::vec4(0.0f);
    glm::vec4 bbox_max = glm::vec4(0.0f);
    std::vector<Triangle> original_triangles;
};

enum class ChestState
{
    CLOSED,
    OPENING,
    OPEN,
    CLOSING,
};

struct ChestInstance
{
    ChestState state = ChestState::CLOSED;
    float current_lid_angle = 0.0f;
    float target_angle = 100.0f;
    float open_timer = 0.0f;
    float speed = 3.0f;
    glm::vec4 bbox_center = glm::vec4(0.0f);
    glm::vec4 bbox_min = glm::vec4(0.0f);
    glm::vec4 bbox_max = glm::vec4(0.0f);
};

struct CobwebInstance
{
    bool broken = false;
    glm::vec4 bbox_center = glm::vec4(0.0f);
    glm::vec4 bbox_min = glm::vec4(0.0f);
    glm::vec4 bbox_max = glm::vec4(0.0f);
};

enum class GhostLadderState
{
    FLOATING,
    FALLING,
    GROUNDED,
};

struct GhostLadderInstance
{
    GhostLadderState state = GhostLadderState::FLOATING;
    float current_y_offset = 0.0f;
    glm::vec4 bbox_center = glm::vec4(0.0f);
    glm::vec4 bbox_min = glm::vec4(0.0f);
    glm::vec4 bbox_max = glm::vec4(0.0f);
    std::vector<Triangle> original_triangles;
    int scene_part_index = -1;
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

CollisionAABB ComputeObbAabb(const CollisionOBB &obb);

static bool OverlapOnAxis(
    const glm::vec4 &v0,
    const glm::vec4 &v1,
    const glm::vec4 &v2,
    const glm::vec4 &axis,
    const glm::vec4 &half_extents);

int FindDoorIndexByBBox(const glm::vec4 &bbox_center, const std::vector<DoorInstance> &doors);

int FindChestIndexByBBox(const glm::vec4 &bbox_center, const std::vector<ChestInstance> &chests);

bool BBoxOverlap(const glm::vec4 &a_min, const glm::vec4 &a_max,
                 const glm::vec4 &b_min, const glm::vec4 &b_max);

#endif // _COLLISION_H
