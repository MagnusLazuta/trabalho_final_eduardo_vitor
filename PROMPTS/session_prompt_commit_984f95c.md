# GHOST LADDER — Registro de alterações

## 1. Estruturas de dados adicionadas

### `CollisionShapeType` (`include/collision.h:36`)
```cpp
enum class CollisionShapeType {
    SOLID,
    GROUND,       // NOVO: chão, detectado por substring "GROUND" no nome
    DOOR,
    NONE,
    WATER,
    GHOST_LADDER, // NOVO: escada fantasma
    LADDER,
    COBWEB_FLOORHOLE,
    VINES,
};
```

### `GhostLadderState` + `GhostLadderInstance` (`include/collision.h`)
```cpp
enum class GhostLadderState { FLOATING, FALLING, GROUNDED };

struct GhostLadderInstance {
    GhostLadderState state = GhostLadderState::FLOATING;
    float current_y_offset = 0.0f;           // offset Y acumulado durante queda
    glm::vec4 bbox_center, bbox_min, bbox_max;
    std::vector<Triangle> original_triangles; // geometria original (Y sem offset)
    int scene_part_index = -1;               // qual cena pertence
};
```

### `PlayerState` (`include/PlayerState.h`)
```cpp
enum class PlayerState { ..., CLIMBING_GHOST_LADDER };
// assinatura do Update() ganhou parâmetro: bool isClimbingGhostLadder
```

### Variáveis globais (`include/globals.h`, `src/globals.cpp`)
```cpp
extern std::vector<GhostLadderInstance> g_GhostLadders;
extern bool g_CollidedWithAGhostLadder;
extern bool g_IsClimbingAGhostLadder;
```

## 2. Fluxo de comportamento

1. **Ghost ladder começa FLOATING** — visível no ar em scene02, sem colisão com player (não está em `g_ScenarioCollisionShapes`), player pode atravessá-la.

2. **Estilingue acerta** → `QueryProjectileCollision` (`main.cpp`) testa AABB do projétil contra `GhostLadderInstance` em FLOATING. Ao acertar: estado muda para `FALLING`.

3. **Queda vertical** → `UpdateGhostLadders` (`main.cpp`) aplica `fall_speed = -15.0` ao `current_y_offset` via `OBB` (yaw=0). Testa triângulo por triângulo contra shapes `GROUND`, filtrando `tri_mid_y < full_center.y` para ignorar paredes:
   - Calcula `best_ground_y` (Y médio mais alto entre triângulos de chão que intersectam).
   - Se encontrado: snap direto — `y_offset = best_ground_y + 0.01 + full_half.y - bbox_center.y`.
   - Estado muda para `GROUNDED`.

4. **Escalada** → `IsCollidingWithGhostLadder` (`movement.cpp`): OBB contra `GhostLadderInstance.original_triangles` (com offset Y aplicado por vértice). Só aceita ghost ladders em estado `GROUNDED`.
   - Tecla **O** inicia/para escalada (mesmo handler de ladder/vine).
   - Velocidade de subida: `2.0` (igual ladder).
   - Auto-stop com `grace_period = 0.3s` quando player sai da área.

5. **Player State** → `PlayerStateMachine::Update` ganhou `isClimbingGhostLadder`. Prioridade: ghost > ladder > vine (ordem de checagem no state machine).

6. **Anti-gravidade** → Em `movement.cpp`, todas as condições que pulam gravidade/ground-snap para ladder/vine também incluem ghost ladder.

7. **Snap** → `SnapPlayerToNearestVine` (`movement.cpp`) estendido para incluir ghost ladders grounded, usando as mesmas regras de projeção XZ.

## 3. Render

- GHOST_LADDER é **pulado no Path 1** do render do map.obj (adicionado à lista de skip junto com DOOR/CHEST/COBWEB_FLOORHOLE).
- **Render separado** entre Path 1 e Path 2: `Matrix_Translate(0, current_y_offset, 0)` aplicado à model matrix, desenha todas as shapes do `g_VirtualScene` com "GHOST_LADDER" no nome, da cena ativa correspondente.
- Debug hitbox: cian (FLOATING), laranja (FALLING), verde-água (GROUNDED).

## 4. Colisão: geometria extraída do map.obj

A geometria de colisão do GHOST_LADDER é extraída **diretamente do `map.obj`** durante o carregamento da cena (sem precisar modificar `collision.obj`):

```cpp
// main.cpp — loop de carregamento das cenas
for (size_t shape = 0; shape < map_model.shapes.size(); ++shape) {
    if (sname.find("GHOST_LADDER") == std::string::npos) continue;
    // constrói CollisionShape com tipo GHOST_LADDER
    part.collision_shapes.push_back(shape_collision);
}
```

Depois, ghost ladders são **filtradas de `g_ScenarioCollisionShapes`** (não bloqueiam movimento do player) e registradas como `GhostLadderInstance` em `g_GhostLadders`.

## 5. GROUND — novo tipo de colisão

### Detecção por nome no parser
Em ambos `BuildCollisionDataFromObjModel` e `BuildCollisionDataIntoVector` (`main.cpp`):
```cpp
else if (model->shapes[shape].name.find("GROUND") != std::string::npos)
    shape_collision.type = CollisionShapeType::GROUND;
```
Antes do `else` que mapeia para SOLID. Também mapeado em `BuildCollisionShapeFromInstance` (objects.json: `"collision_type": "GROUND"`).

### Comportamento
- **Player/Enemies**: GROUND bloqueia movimento como SOLID (adicionado em `IsPositionFree`, `IsPositionFreeWalkThrough`, `g_PlayerCubeColliding`, ground checks do O key handler, `enemies.cpp`).
- **Projétil do estilingue**: GROUND também para o projétil.
- **Ghost ladder**: testa exclusivamente contra GROUND na detecção de chão.

## 6. Mapeamento de nomes — ordem crítica

No parser de colisão, a ordem dos checks é importante porque usa `std::string::find` (substring):

```
DOOR → COBWEB_FLOORHOLE → VINES → GHOST_LADDER → LADDER → WATER → GROUND → else SOLID
```

`GHOST_LADDER` precisa vir **antes** de `LADDER` (porque "GHOST_LADDER" contém "LADDER").

## 7. Problemas encontrados e resolvidos

| Problema | Causa | Solução |
|----------|-------|---------|
| Ghost ladder parava no ar | Full AABB batia em paredes (shapes monolíticas do Deku Tree) | Filtro `tri_mid_y < full_center.y` por triângulo, não por shape |
| Ghost ladder caía pra fora do mapa | Foot detection com 30% de largura escapava entre triângulos | Teste com OBB completo da ghost ladder |
| Crash: `Produto escalar não definido para pontos` | `foot_center.w = 0` fazia `dotproduct` quebrar após subtração com vértice `w=1` | `foot_center.w = 1.0f` |
| `groundShapes=0` no runtime | Chão não tinha "GROUND" no nome em `collision.obj` | Adicionado debug de carga mostrando contagem por tipo |
| Build quebrou (syntax) | Código duplicado após edit com `replaceAll` | Removido bloco duplicado manualmente |

## 8. Debug removidos

Para reduzir ruído no console, foram removidos:
- COBWEB DEBUG (spam de `yFree=false velY=...`)
- CLIMB MOVE (posição durante escalada)
- CLIMB COL (estado de colisão durante escalada)
- SNAP VINE (snap ao encostar em vine)
- DOOR DEBUG (bbox das portas)
- Todos os `printf` do O KEY handler (started/stopped vine/ladder/ghost)

## 9. Arquivos modificados

| Arquivo | Alterações |
|---------|------------|
| `include/collision.h` | GHOST_LADDER + GROUND no enum, GhostLadderState, GhostLadderInstance |
| `include/globals.h` | g_GhostLadders, g_CollidedWithAGhostLadder, g_IsClimbingAGhostLadder |
| `src/globals.cpp` | Definições das globais |
| `include/PlayerState.h` | CLIMBING_GHOST_LADDER, Update() com isClimbingGhostLadder |
| `src/PlayerState.cpp` | CLIMBING_GHOST_LADDER no GetAnimationIndex() e Update() |
| `src/main.cpp` | Parser (GHOST_LADDER + GROUND), extração de colisão do map.obj, QueryProjectileCollision, UpdateGhostLadders, UpdateSlingshotProjectile (INTERACTIVE_OBJECT), O key handler, P key handler, PlayerStateMachine.Update, render separado, debug hitbox, filtro de g_ScenarioCollisionShapes |
| `src/movement.cpp` | IsCollidingWithGhostLadder, detecção de escalada, climbing state, auto-stop, snap, anti-gravidade, IsPositionFree (GROUND) |
| `src/enemies.cpp` | GROUND como bloqueador de movimento e projétil |
