#include "collision.h"
#include "types.h"
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

CollisionShapeType CollidesWithScenarioAabb(const glm::vec4 &center, const glm::vec4 &half_extents, const std::vector<CollisionShape> &g_ScenarioCollisionShapes)
{
    const glm::vec4 cube_min = center - half_extents;
    const glm::vec4 cube_max = center + half_extents;

    for (size_t shape_index = 0; shape_index < g_ScenarioCollisionShapes.size(); ++shape_index)
    {
        const CollisionShape &shape = g_ScenarioCollisionShapes[shape_index];

        // Broad phase: filtra apenas formas com sobreposição de AABB.
        CollisionAABB box_aabb = {cube_min, cube_max};
        CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
        if (!AabbAabbIntersect(box_aabb, shape_aabb))
            continue;

        // Narrow phase: testa triângulo-a-triângulo (AABB do cubo vs triângulo).
        for (size_t triangle_index = 0; triangle_index < shape.triangles.size(); ++triangle_index)
        {
            if (TriangleIntersectsAabb(shape.triangles[triangle_index], center, half_extents))
                return shape.type;
        }
    }

    return CollisionShapeType::NONE;
}

bool TriangleIntersectsAabb(const Triangle &triangle, const glm::vec4 &box_center, const glm::vec4 &box_half_extents)
{
    // Colocamos tudo no referencial local da AABB (caixa centrada na origem).
    const glm::vec4 v0 = triangle.v1 - box_center;
    const glm::vec4 v1 = triangle.v2 - box_center;
    const glm::vec4 v2 = triangle.v3 - box_center;

    const glm::vec4 e0 = v1 - v0;
    const glm::vec4 e1 = v2 - v1;
    const glm::vec4 e2 = v0 - v2;
    const glm::vec4 edges[3] = {e0, e1, e2};
    const glm::vec4 axis_basis[3] =
        {
            glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
            glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
        };

    // SAT: 3 eixos da AABB
    if (std::max(v0.x, std::max(v1.x, v2.x)) < -box_half_extents.x || std::min(v0.x, std::min(v1.x, v2.x)) > box_half_extents.x)
        return false;
    if (std::max(v0.y, std::max(v1.y, v2.y)) < -box_half_extents.y || std::min(v0.y, std::min(v1.y, v2.y)) > box_half_extents.y)
        return false;
    if (std::max(v0.z, std::max(v1.z, v2.z)) < -box_half_extents.z || std::min(v0.z, std::min(v1.z, v2.z)) > box_half_extents.z)
        return false;

    // SAT: 9 eixos cruzamento aresta-triângulo X eixo-caixa
    for (int edge_index = 0; edge_index < 3; ++edge_index)
    {
        for (int axis_index = 0; axis_index < 3; ++axis_index)
        {
            const glm::vec4 axis4 = crossproduct(
                edges[edge_index],
                axis_basis[axis_index]);
            const glm::vec4 axis(axis4.x, axis4.y, axis4.z, 0.0f);
            if (!OverlapOnAxis(v0, v1, v2, axis, box_half_extents))
                return false;
        }
    }

    // SAT: eixo normal do triângulo
    const glm::vec4 tri_normal4 = crossproduct(e0, e1);
    const glm::vec4 tri_normal(tri_normal4.x, tri_normal4.y, tri_normal4.z, 0.0f);
    if (!OverlapOnAxis(v0, v1, v2, tri_normal, box_half_extents))
        return false;

    return true;
}

CollisionShapeType CollidesWithScenario(const glm::vec4 &cube_center, const std::vector<CollisionShape> &g_ScenarioCollisionShapes, const glm::vec4 &g_PlayerCubeHalfExtents)
{
    return CollidesWithScenarioAabb(cube_center, g_PlayerCubeHalfExtents, g_ScenarioCollisionShapes);
}

bool IsCollidingWithType(const glm::vec4 &center, const glm::vec4 &half_extents, const std::vector<CollisionShape> &g_ScenarioCollisionShapes, CollisionShapeType type)
{
    const glm::vec4 cube_min = center - half_extents;
    const glm::vec4 cube_max = center + half_extents;

    for (size_t shape_index = 0; shape_index < g_ScenarioCollisionShapes.size(); ++shape_index)
    {
        const CollisionShape &shape = g_ScenarioCollisionShapes[shape_index];
        if (shape.type != type)
            continue;

        CollisionAABB box_aabb = {cube_min, cube_max};
        CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
        if (!AabbAabbIntersect(box_aabb, shape_aabb))
            continue;

        for (size_t triangle_index = 0; triangle_index < shape.triangles.size(); ++triangle_index)
        {
            if (TriangleIntersectsAabb(shape.triangles[triangle_index], center, half_extents))
                return true;
        }
    }
    return false;
}

bool GetVinesNormal(const glm::vec4 &center, const glm::vec4 &half_extents, const std::vector<CollisionShape> &g_ScenarioCollisionShapes, glm::vec4 &out_normal)
{
    const glm::vec4 cube_min = center - half_extents;
    const glm::vec4 cube_max = center + half_extents;

    for (size_t shape_index = 0; shape_index < g_ScenarioCollisionShapes.size(); ++shape_index)
    {
        const CollisionShape &shape = g_ScenarioCollisionShapes[shape_index];
        if (shape.type != CollisionShapeType::VINES)
            continue;

        CollisionAABB box_aabb = {cube_min, cube_max};
        CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
        if (!AabbAabbIntersect(box_aabb, shape_aabb))
            continue;

        for (size_t triangle_index = 0; triangle_index < shape.triangles.size(); ++triangle_index)
        {
            const Triangle &triangle = shape.triangles[triangle_index];
            if (TriangleIntersectsAabb(triangle, center, half_extents))
            {
                glm::vec4 e1 = triangle.v2 - triangle.v1;
                glm::vec4 e2 = triangle.v3 - triangle.v1;
                glm::vec4 n = crossproduct(e1, e2);
                float len = std::sqrt(dotproduct(n, n));
                if (len > 1e-6f)
                {
                    out_normal = n / len;
                    out_normal.w = 0.0f;
                    return true;
                }
            }
        }
    }
    return false;
}

static bool OverlapOnAxis(
    const glm::vec4 &v0,
    const glm::vec4 &v1,
    const glm::vec4 &v2,
    const glm::vec4 &axis,
    const glm::vec4 &half_extents)
{
    const float eps = 1e-7f;
    if (dotproduct(axis, axis) < eps)
        return true;

    const float p0 = dotproduct(v0, axis);
    const float p1 = dotproduct(v1, axis);
    const float p2 = dotproduct(v2, axis);
    const float tri_min = std::min(p0, std::min(p1, p2));
    const float tri_max = std::max(p0, std::max(p1, p2));

    const float r =
        half_extents.x * std::fabs(axis.x) +
        half_extents.y * std::fabs(axis.y) +
        half_extents.z * std::fabs(axis.z);

    return !(tri_min > r || tri_max < -r);
}

static bool OverlapOnAxisObb(
    const glm::vec4 &v0,
    const glm::vec4 &v1,
    const glm::vec4 &v2,
    const glm::vec4 &axis,
    const glm::vec4 *obb_axes,
    const glm::vec4 &half_extents)
{
    const float eps = 1e-7f;
    if (dotproduct(axis, axis) < eps)
        return true;

    const float p0 = dotproduct(v0, axis);
    const float p1 = dotproduct(v1, axis);
    const float p2 = dotproduct(v2, axis);
    const float tri_min = std::min(p0, std::min(p1, p2));
    const float tri_max = std::max(p0, std::max(p1, p2));

    const float r =
        half_extents.x * std::fabs(dotproduct(obb_axes[0], axis)) +
        half_extents.y * std::fabs(dotproduct(obb_axes[1], axis)) +
        half_extents.z * std::fabs(dotproduct(obb_axes[2], axis));

    return !(tri_min > r || tri_max < -r);
}

bool TriangleIntersectsObb(const Triangle &triangle, const CollisionOBB &obb)
{
    const float c = std::cos(obb.yaw);
    const float s = std::sin(obb.yaw);

    const glm::vec4 obb_axes[3] = {
        glm::vec4(c, 0.0f, -s, 0.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
        glm::vec4(s, 0.0f, c, 0.0f)};

    const glm::vec4 v0 = triangle.v1 - obb.center;
    const glm::vec4 v1 = triangle.v2 - obb.center;
    const glm::vec4 v2 = triangle.v3 - obb.center;

    const glm::vec4 e0 = v1 - v0;
    const glm::vec4 e1 = v2 - v1;
    const glm::vec4 e2 = v0 - v2;
    const glm::vec4 edges[3] = {e0, e1, e2};

    if (!OverlapOnAxisObb(v0, v1, v2, obb_axes[0], obb_axes, obb.half_extents))
        return false;
    if (!OverlapOnAxisObb(v0, v1, v2, obb_axes[1], obb_axes, obb.half_extents))
        return false;
    if (!OverlapOnAxisObb(v0, v1, v2, obb_axes[2], obb_axes, obb.half_extents))
        return false;

    const glm::vec4 tri_normal4 = crossproduct(e0, e1);
    const glm::vec4 tri_normal(tri_normal4.x, tri_normal4.y, tri_normal4.z, 0.0f);
    if (!OverlapOnAxisObb(v0, v1, v2, tri_normal, obb_axes, obb.half_extents))
        return false;

    for (int edge_index = 0; edge_index < 3; ++edge_index)
    {
        for (int axis_index = 0; axis_index < 3; ++axis_index)
        {
            const glm::vec4 axis4 = crossproduct(edges[edge_index], obb_axes[axis_index]);
            const glm::vec4 axis(axis4.x, axis4.y, axis4.z, 0.0f);
            if (!OverlapOnAxisObb(v0, v1, v2, axis, obb_axes, obb.half_extents))
                return false;
        }
    }

    return true;
}

CollisionAABB ComputeObbAabb(const CollisionOBB &obb)
{
    const float c = std::abs(std::cos(obb.yaw));
    const float s = std::abs(std::sin(obb.yaw));

    const glm::vec4 extent(
        obb.half_extents.x * c + obb.half_extents.z * s,
        obb.half_extents.y,
        obb.half_extents.x * s + obb.half_extents.z * c,
        0.0f);

    return {obb.center - extent, obb.center + extent};
}

CollisionShapeType CollidesWithScenarioObb(const CollisionOBB &obb, const std::vector<CollisionShape> &g_ScenarioCollisionShapes)
{
    const CollisionAABB obb_aabb = ComputeObbAabb(obb);

    for (size_t shape_index = 0; shape_index < g_ScenarioCollisionShapes.size(); ++shape_index)
    {
        const CollisionShape &shape = g_ScenarioCollisionShapes[shape_index];

        CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
        if (!AabbAabbIntersect(obb_aabb, shape_aabb))
            continue;

        for (size_t triangle_index = 0; triangle_index < shape.triangles.size(); ++triangle_index)
        {
            if (TriangleIntersectsObb(shape.triangles[triangle_index], obb))
                return shape.type;
        }
    }

    return CollisionShapeType::NONE;
}

bool IsCollidingWithTypeObb(const CollisionOBB &obb, const std::vector<CollisionShape> &g_ScenarioCollisionShapes, CollisionShapeType type)
{
    const CollisionAABB obb_aabb = ComputeObbAabb(obb);

    for (size_t shape_index = 0; shape_index < g_ScenarioCollisionShapes.size(); ++shape_index)
    {
        const CollisionShape &shape = g_ScenarioCollisionShapes[shape_index];
        if (shape.type != type)
            continue;

        CollisionAABB shape_aabb = {shape.bbox_min, shape.bbox_max};
        if (!AabbAabbIntersect(obb_aabb, shape_aabb))
            continue;

        for (size_t triangle_index = 0; triangle_index < shape.triangles.size(); ++triangle_index)
        {
            if (TriangleIntersectsObb(shape.triangles[triangle_index], obb))
                return true;
        }
    }
    return false;
}

int FindDoorIndexByBBox(const glm::vec4 &bbox_center, const std::vector<DoorInstance> &doors)
{
    for (size_t i = 0; i < doors.size(); ++i)
    {
        float dx = doors[i].bbox_center.x - bbox_center.x;
        float dy = doors[i].bbox_center.y - bbox_center.y;
        float dz = doors[i].bbox_center.z - bbox_center.z;
        if (dx * dx + dy * dy + dz * dz < 0.01f)
            return (int)i;
    }
    return -1;
}

bool BBoxOverlap(const glm::vec4 &a_min, const glm::vec4 &a_max,
                 const glm::vec4 &b_min, const glm::vec4 &b_max)
{
    return (a_min.x <= b_max.x && a_max.x >= b_min.x) &&
           (a_min.y <= b_max.y && a_max.y >= b_min.y) &&
           (a_min.z <= b_max.z && a_max.z >= b_min.z);
}