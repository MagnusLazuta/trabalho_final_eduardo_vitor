#ifndef _COLLISION_H
#define _COLLISION_H

#include <glm/vec4.hpp>
#include <vector>
#include "matrices.h"

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

// Broad Phase: AABB vs AABB
bool AabbAabbIntersect(CollisionAABB a, CollisionAABB b);

// Broad Phase: Ray vs AABB
bool RayAabbIntersect(CollisionRay r, CollisionAABB a, float &t);

// Narrow Phase: Ray vs Triangle
    bool RayTriangleIntersect(CollisionRay r, CollisionTriangle tri, float &t);

#endif // _COLLISION_H
