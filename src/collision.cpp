#include "collision.h"
#include <algorithm>
#include <cmath>

bool AabbAabbIntersect(CollisionAABB a, CollisionAABB b)
{
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

bool RayAabbIntersect(CollisionRay r, CollisionAABB a, float &t)
{
    float tmin = -INFINITY, tmax = INFINITY;

    if (r.direction.x != 0.0f)
    {
        float tx1 = (a.min.x - r.origin.x) / r.direction.x;
        float tx2 = (a.max.x - r.origin.x) / r.direction.x;
        tmin = std::max(tmin, std::min(tx1, tx2));
        tmax = std::min(tmax, std::max(tx1, tx2));
    }
    else if (r.origin.x < a.min.x || r.origin.x > a.max.x)
        return false;

    if (r.direction.y != 0.0f)
    {
        float ty1 = (a.min.y - r.origin.y) / r.direction.y;
        float ty2 = (a.max.y - r.origin.y) / r.direction.y;
        tmin = std::max(tmin, std::min(ty1, ty2));
        tmax = std::min(tmax, std::max(ty1, ty2));
    }
    else if (r.origin.y < a.min.y || r.origin.y > a.max.y)
        return false;

    if (r.direction.z != 0.0f)
    {
        float tz1 = (a.min.z - r.origin.z) / r.direction.z;
        float tz2 = (a.max.z - r.origin.z) / r.direction.z;
        tmin = std::max(tmin, std::min(tz1, tz2));
        tmax = std::min(tmax, std::max(tz1, tz2));
    }
    else if (r.origin.z < a.min.z || r.origin.z > a.max.z)
        return false;

    t = tmin;
    return tmax >= tmin && tmax >= 0.0f;
}

bool RayTriangleIntersect(CollisionRay r, CollisionTriangle tri, float &t)
{
    const float EPSILON = 0.000001f;
    glm::vec4 edge1 = tri.v1 - tri.v0; // w = 0
    glm::vec4 edge2 = tri.v2 - tri.v0; // w = 0
    
    // Garantir que são vetores para dotproduct/crossproduct
    edge1.w = 0.0f;
    edge2.w = 0.0f;
    glm::vec4 h = crossproduct(r.direction, edge2);
    float a = dotproduct(edge1, h);

    if (a > -EPSILON && a < EPSILON)
        return false; // Ray is parallel to triangle

    float f = 1.0f / a;
    glm::vec4 s = r.origin - tri.v0;
    s.w = 0.0f;
    float u = f * dotproduct(s, h);

    if (u < 0.0f || u > 1.0f)
        return false;

    glm::vec4 q = crossproduct(s, edge1);
    float v = f * dotproduct(r.direction, q);

    if (v < 0.0f || u + v > 1.0f)
        return false;

    float t_hit = f * dotproduct(edge2, q);

    if (t_hit > EPSILON)
    {
        t = t_hit;
        return true;
    }

    return false;
}
