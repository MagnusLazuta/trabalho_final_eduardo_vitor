# Conversation Log - Collision & Climbing Fixes

## 1. Player falling through map in new scenes

**User:** existe em algumas partes do novo mapa que o personagem tá caindo pra fora. A questão do climbing foi resolvida

**Assistant:** Investigated the collision system. Found the root cause: scene AABB was computed from `map.obj` vertices, but collision data came from `collision.obj` which was at completely different positions. The scene detection used the map AABB, so the player was detected in the scene, but collision shapes were miles away. Added XZ-only fallback for scene detection and AABB union (map + collision) to prevent the death spiral where collision disappears and player falls forever.

## 2. User asks about collision keywords

**User:** QUAIS PALAVRAS CHAVE EU TENHO QUE COLOCAR PARA ELE IDENTIFICAR COMO UM SOLIDO OU UM CHÃO ?

**Assistant:** The collision shape classification is done by string matching on `shape.name`:
- `DOOR` → CollisionShapeType::DOOR
- `VINES` → CollisionShapeType::VINES
- `LADDER` → CollisionShapeType::LADDER
- `WATER` → CollisionShapeType::WATER
- **Anything else** → CollisionShapeType::SOLID (default)

For ground/solid, no keyword needed. Any shape name that doesn't contain DOOR/VINES/LADDER/WATER is automatically SOLID.

## 3. User questions collision implementation

**User:** você tem certeza que está implementando de forma certa a colisão ? pq isso só acontece nessas cenas que tu implementou hj e não na scene00

**Assistant:** Investigated and found the root cause: the `collision.obj` files for the new scenes were exported at completely different positions than the `map.obj` files. For example:
- scene00: map.obj and collision.obj at same position ✓
- scene02: map.obj at (23, 12, 23) but collision.obj at (21, 6, 28) ✗
- scene07: map.obj at (3, -11, 52) but collision.obj at (16, -16, 40) ✗
- scene09: map.obj at (-23, -38, 22) but collision.obj at (0.6, -36, 6) ✗

The scene AABB was computed from map.obj, so player was detected in the scene, but collision shapes were at a completely different location. Player walks on visual floor but collision geometry is somewhere else → falls through.

## 4. User asks to apply offset in collision.obj directly

**User:** eu quero que aplique o alinhamento DIRETO NO COLLISION NÃO NO MEU CÓDIGO DO JOGO

**Assistant:** Calculated the exact offset between map.obj and collision.obj AABB centers for each scene:
- scene02: already fixed by user
- scene07: OFFSET=(12.944, 12.221, 12.424)
- All others: aligned

Applied the offset directly to scene07's collision.obj vertices using a Python script. Verified centers now match: (3.0, -14.6, 26.0).

## 5. User re-exports scene07 in Blender

**User:** refiz de novo o o processo no blender, vê se ficou alinhado

**Assistant:** Verified scene07 alignment after re-export. Result:
```
map.obj:       X=[-11.0,17.1] Y=[-24.1,-5.2] Z=[-2.6,54.7]
collision.obj: X=[-11.0,17.1] Y=[-24.1,-5.2] Z=[-2.6,54.7]
center diff:   (0.000, 0.000, 0.000)
aligned: True
```
Perfect alignment achieved.

## 6. Crash on scene02 loading

**User:** [shared crash log with texture path error]

**Assistant:** The crash was caused by `collision.mtl` files containing absolute paths from Blender (e.g., `/home/edulazuta/...`). The loader was prepending the scene directory to these absolute paths, creating broken paths. Fixed by removing `mtllib` from all `collision.obj` files since collision geometry doesn't need textures.

## 7. Debug visualization request

**User:** não faz seguinte eu to indo pra scene02 e fica caindo perto da vines ppqpq

**Assistant:** Added debug visualization of collision shapes:
- Bright green = Player OBB
- Dark green = SOLID collision shapes (new)
- Yellow = Ladder collision shapes
- Magenta = Vine collision shapes

Toggle with F1. This shows where collision exists and where there are gaps.

## 8. Floor gap analysis for scene02

**User:** [shared log showing player falling from Y=9.6 to Y=-1.9 near vines]

**Assistant:** Analyzed the collision geometry. Found that while AABBs cover the area, the actual triangles have gaps. Specifically identified 12 test points in the gap area (X=29-31, Z=26.5-28.5) that had no floor coverage.

Added 6 new vertices and 10 new triangles to `scene02/collision.obj` to fill the gap:
```
New vertices: (30.5,6.2,27.0), (30.5,6.2,28.0), (29.5,6.2,28.5),
              (31.0,6.2,26.5), (31.0,6.2,27.5), (31.0,6.2,28.5)
```
All 12 test points verified as covered after the fix.

## 9. Player flying indefinitely on vines/ladders

**User:** o link está subindo indefinidamente quando está numa vine ou numa ladder

**Assistant:** The problem was `yFree = true` during climbing, which removed ALL collision checks. Fixed by using `IsPositionFreeForClimbing` with small extents `(0.1, 0.1, 0.1)` during climbing. This skips VINES/LADDER shapes but still detects SOLID geometry (floor/ceiling).

## 10. Player keeps flying after leaving vine

**User:** o problema é que se não está mais detectando colisão com uma vine ou ladder, ele acaba voando e subindo quando aperta W mesmo depois de pela vine

**Assistant:** The auto-stop had `!tryingToClimb` condition, meaning it only stopped climbing when W/S was NOT pressed. If the player held W while leaving the vine, climbing never stopped. Fixed by removing the `!tryingToClimb` condition — now auto-stop triggers after 0.3s grace period regardless of input.

## Summary of all changes

### Code changes (src/movement.cpp)
1. Climbing Y movement: uses `IsPositionFreeForClimbing` with small extents (0.1, 0.1, 0.1) instead of `yFree = true`
2. Auto-stop: removed `!tryingToClimb` condition, stops after grace period regardless of input
3. Added `SnapPlayerToNearestVine()` function (later removed from main.cpp calls)
4. Added FALL debug logging

### Code changes (src/main.cpp)
1. Scene detection: XZ-only fallback when player falls below scene AABB Y range
2. Scene AABB: union of map.obj and collision.obj AABBs
3. Debug visualization: added SOLID shape rendering (dark green wireframe)
4. Removed `SnapPlayerToNearestVine()` calls from O key handler
5. Added ground state check when starting climb

### Code changes (include/movement.h)
1. Added `SnapPlayerToNearestVine()` declaration (later removed)

### Asset changes
1. Removed `mtllib` from all collision.obj files (prevents texture path crashes)
2. scene02/collision.obj: added 6 vertices + 10 triangles to fill floor gap near vine
3. scene07/collision.obj: user re-exported from Blender with correct alignment
