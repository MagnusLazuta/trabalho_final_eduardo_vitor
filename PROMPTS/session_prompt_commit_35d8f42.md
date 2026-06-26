# Conversation Log — FPS Optimization in Scene00

## User Prompt
> "percebi que o fps na scene00 tá 30 fps, arrume"

## Investigation

### FPS Cap Analysis
- The game loop in `src/main.cpp:4762` caps FPS at **60** (`1.0 / 60.0`).
- No `glfwSwapInterval` call exists — VSync is at driver default.
- No scene00-specific FPS configuration found.

### Root Cause Found: Enemy Raycast Brute-Force

Scene00 has **4 small spiders** (Skullwalltula). Each spider fires **10 raycasts per frame**:
- `AlignSkullwalltulaToNearbyWall`: 8 horizontal rays to find nearest wall
- `MoveSkullwalltulaVertically`: 2 vertical rays for floor/ceiling detection

Each raycast iterates over **~1,308 SOLID+DOOR triangles** with zero spatial partitioning:
- Per-triangle normals are **recomputed every frame** (crossproduct + normalize)
- **No AABB broad-phase** — `RayAabbIntersect` existed in `collision.cpp:13` but was never called
- **No spatial partitioning** (no grid, BVH, octree)

**Cost per frame (4 spiders): ~52,320 Moller-Trumbore tests** (up to 200k+ with adjacent scenes).

## Optimizations Applied

### 1. Precompute Triangle Normals (`include/types.h`, `src/main.cpp`)
- Added `glm::vec4 normal` field to `Triangle` struct
- Normal computed once in `BuildCollisionDataIntoVector` during scene load
- Eliminates ~14 FP operations per triangle per ray that were repeated every frame

### 2. AABB Broad-Phase (`src/enemies.cpp`)
- Added `RayAabbIntersect` check before per-triangle loop in both spider raycast functions
- Skips entire collision shapes whose AABB is not intersected by the ray
- Estimated **70% reduction** in triangle tests (most rays miss most shapes)

### 3. Frame Skip for Wall Alignment (`src/enemies.cpp`)
- `AlignSkullwalltulaToNearbyWall` runs only every **2nd frame** (50% reduction)
- Safe because wall alignment is cosmetic (yaw/orientation only)
- Spider x/z position is clamped to spawn position anyway

### 4. Ray Distance Limit (`src/enemies.cpp`)
- Vertical rays limited to `SKULLWALLTULA_VERTICAL_RANGE` (1.0 units)
- Horizontal rays already limited to `SKULLWALLTULA_WALL_PROBE_DISTANCE` (2.5 units) via `closest_distance`

## Files Changed

| File | Change |
|------|--------|
| `include/types.h:8` | Added `glm::vec4 normal` to `Triangle` struct |
| `src/main.cpp:5900` | Normal computed and stored once during triangle creation |
| `src/enemies.cpp:593` | `MoveSkullwalltulaVertically`: AABB broad-phase, precomputed normal, distance limit |
| `src/enemies.cpp:643` | `AlignSkullwalltulaToNearbyWall`: AABB broad-phase, precomputed normal, runs every 2nd frame |

## Build Result
- Compiles cleanly (zero warnings from changed code)
- `bin/Linux/main` linked successfully

## Estimated Impact
**~75-85% reduction** in enemy raycast CPU cost in scene00.
