# Conversation Log - Alteracoes no C++ (src/main.cpp)

## 1. Sistema de Object Instancing via JSON

**Prompt:** Criar um sistema onde cada cena tem seu `objects.json` com objetos instanciaveis, compartilhando `.obj` unicos.

**Alteracoes em `src/main.cpp`:**

- **`SceneObjectInstance`** (struct): armazena model_path, type, position, rotation, scale, collision_type, aabb_min, aabb_max, has_aabb
- **`g_SceneObjectInstances`** (map<int, vector<SceneObjectInstance>>): armazena instancias por cena
- **`LoadSceneObjectInstances()`**: parse manual do JSON (padrao identico ao enemy_spawns.json, sem nlohmann/json)
- **`SaveAabbToSceneJSON()`**: reescreve o JSON inteiro com AABB atualizado
- **`AutoComputeAABB()`**: carrega .obj, itera vertices, aplica escala/rotacao/posicao, calcula bounding box
- **`LoadOrGetCachedObjModel()`**: cache de .obj em `g_ObjModelCache` para nao recarregar modelos compartilhados
- **`BuildCollisionShapeFromInstance()`**: gera shape de colisao a partir da instancia JSON

---

## 2. Bug: Crash por indices de faces invalidos

**Prompt:** "a colisao com a porta parou de funcionar e o que voce fez com as vines?"

**Diagnostico:** Quando DOOR/VINES foram removidos dos `collision.obj`, os vertices foram deletados mas os indices das faces nao foram reajustados. Resultado: `Assertion '__n < this->size()' failed` em `vector<float>`.

**Cenas afetadas:** scene00 (DOOR), scene02/03/07/10 (VINES)

**Exemplo do bug (scene00):**
```
4736 vertices (depois de remover 12 do DOOR)
Faces referenciando indices 4737-4748 (ja deletados)
=> 16 erros de indices invalidos
```

**Fix:** Restaurar collision.obj do git + reescrever com reindexacao correta dos vertices.

---

## 3. Fix: BuildCollisionShapeFromInstance usa triangulos reais

**Prompt:** "aabb so e pra possiveis colisoes"

**Problema anterior:** `BuildCollisionShapeFromInstance` gerava 12 triangulos fake de uma caixa AABB. A colisao era uma caixa solida ao redor do objeto, nao a geometria real.

**Codigo antigo (errado):**
```cpp
// 12 triangulos (6 faces * 2 triangulos cada) - CAIXA FAKE
shape.triangles.push_back({mn.x, mn.y, mx.z, mx.x, mn.y, mx.z, mx.x, mx.y, mx.z});
// ... mais 11 triangulos de caixa
```

**Codigo novo (correto):**
```cpp
CollisionShape BuildCollisionShapeFromInstance(const SceneObjectInstance& inst)
{
    // Constroi model matrix: T * Ry * Rx * Rz * S
    glm::mat4 model_matrix = T * Ry * Rx * Rz * S;

    // Carrega o .obj real e extrai triangulos
    ObjModel* obj_model = LoadOrGetCachedObjModel(inst.model_path);
    std::vector<CollisionShape> temp_shapes;
    glm::vec4 temp_min, temp_max;
    BuildCollisionDataIntoVector(obj_model, model_matrix, temp_shapes, temp_min, temp_max);

    // Usa triangulos reais com o tipo do JSON
    for (auto& s : temp_shapes)
    {
        s.type = shape.type;
        shape.triangles.insert(shape.triangles.end(), s.triangles.begin(), s.triangles.end());
        // atualiza bbox_min/bbox_max
    }
}
```

**Resultado:** Colisao agora usa a geometria fina do `.obj` (ex: painel da porta), nao uma caixa solida.

---

## 4. Bug: Outros modelos subindo ao apertar ENTER

**Prompt:** "outros modelos que nao door estao subindo quando clico ENTER"

**Problema:** O render loop verificava se o **centro de qualquer objeto** estava dentro do AABB da porta e aplicava `door_y_offset`. Tudo dentro do bounding box subia junto.

**Codigo antigo (errado):**
```cpp
// Linha 3248 - verificava centro de QUALQUER objeto
const SceneObject &obj = g_VirtualScene[name];
glm::vec4 obj_center = (obj.bbox_min + obj.bbox_max) * 0.5f;
for (const auto &door : g_Doors)
{
    if (door.current_y_offset > 0.01f &&
        obj_center.x >= door.bbox_min.x && obj_center.x <= door.bbox_max.x &&
        obj_center.y >= door.bbox_min.y && obj_center.y <= door.bbox_max.y &&
        obj_center.z >= door.bbox_min.z && obj_center.z <= door.bbox_max.z)
    {
        door_y_offset = door.current_y_offset;  // QUALQUER objeto dentro do AABB sobe
    }
}
```

**Codigo novo (correto):**
```cpp
// So aplica a objetos com "DOOR" no nome
std::string name_str(name);
bool is_door_mesh = (name_str.find("DOOR") != std::string::npos ||
                     name_str.find("door") != std::string::npos);
if (is_door_mesh)
{
    for (const auto &door : g_Doors)
    {
        if (door.current_y_offset > 0.01f)
        {
            door_y_offset = door.current_y_offset;
            break;
        }
    }
}
```

Tambem corrigido na secao de renderizacao de objetos JSON (linha ~3305): removida verificacao de proximidade `distance < 2.0f` que falhava porque `inst.position` era `[0,0,0]` mas `door.bbox_center` era `~[9.9, 10.0, 9.9]`.

---

## 5. Arquivos collision.obj restaurados

| Cena    | Objeto removido | Vertices restaurados | Erros antes | Erros depois |
|---------|----------------|---------------------|-------------|--------------|
| scene00 | DOOR           | 12                  | 16          | 0            |
| scene02 | VINES          | 60                  | 117         | 0            |
| scene03 | VINES          | 179                 | 195         | 0            |
| scene07 | VINES          | 68                  | 88          | 0            |
| scene10 | VINES          | 62                  | 98          | 0            |

Todos restaurados via `git checkout HEAD -- assets/scenes/*/collision.obj`.

---

## 6. objects.json para scene00

**Prompt:** "coloque no objects.json apenas a door para testar"

**Conteudo atual (`assets/scenes/scene00/objects.json`):**
```json
{
  "objects": [
    {
      "model": "door.obj",
      "type": "DOOR",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "scale": [1, 1, 1],
      "interactive": true,
      "collision_type": "DOOR"
    }
  ]
}
```

**Nota:** Position `[0,0,0]` porque `door.obj` ja tem coordenadas de mundo baked in. Se fosse modelo local, position seria a posicao world.

---

## 7. Resumo das funcoes modificadas/criadas

| Funcao | Linha | Descricao |
|--------|-------|-----------|
| `LoadSceneObjectInstances()` | ~1050 | Parse manual do objects.json |
| `SaveAabbToSceneJSON()` | ~1070 | Reescreve JSON com AABB |
| `AutoComputeAABB()` | ~1101 | Calcula AABB a partir do .obj |
| `BuildCollisionShapeFromInstance()` | ~1163 | Triangulos reais do .obj (reescrevido) |
| `LoadOrGetCachedObjModel()` | ~1040 | Cache de modelos .obj |
| Render loop (legado) | ~3248 | Fix: door_y_offset so para DOOR |
| Render loop (JSON) | ~3305 | Fix: removida verificacao de proximidade |
