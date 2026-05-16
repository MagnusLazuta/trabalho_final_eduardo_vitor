# Sessão: Implementação de Logs de Colisão e Correções de Movimentação

## 1. Objetivo Inicial
- Adicionar no terminal as coordenadas do personagem e do triângulo com o qual ele colidiu no cenário.

## 2. Implementações Realizadas

### Logs de Colisão
- **Arquivo**: `src/collision.cpp`
- **Mudança**: Adicionado `printf` na função `CollidesWithScenarioAabb` para imprimir as coordenadas `(X, Y, Z)` do personagem (parâmetro `center`) e os vértices `v1, v2, v3` do triângulo colidido.
- **Arquivo**: `include/collision.h`
- **Mudança**: Corrigida a declaração de `CollidesWithScenarioAabb` para incluir o parâmetro faltante `g_ScenarioCollisionShapes`, garantindo consistência com a implementação.

### Correção de "Colisões Invisíveis" (Desalinhamento de Mesh)
- **Problema**: O personagem colidia com paredes invisíveis ou em lugares vazios.
- **Causa**: Uma lógica de alinhamento automático em `src/main.cpp` tentava re-centralizar e re-escalar o modelo `collision.obj` com base no `map.obj`. Se os modelos tivessem dimensões diferentes (ex: colisão simplificada), o alinhamento distorcia a posição das colisões.
- **Solução**: Desativado o alinhamento automático em `src/main.cpp`, definindo `collision_alignment = Matrix_Identity()`. Isso assume que os modelos já estão alinhados no espaço do software de modelagem (Blender, etc).

### Correção de Bloqueio no Chão (Movimentação Fluida)
- **Problema**: O personagem travava ao tentar andar horizontalmente enquanto estava no chão.
- **Causa**: A parte inferior da AABB do personagem (`Y = -0.02`) intersecionava com o chão (`Y = 0.0`), e o sistema de colisão interpretava o chão como um obstáculo lateral.
- **Solução**: Modificado `src/movement.cpp` para usar uma margem de altura (`horizontal_test_margin = 0.1f`) nos testes de movimento horizontal (eixos X e Z). O personagem é "erguido" levemente durante o teste para ignorar o piso, mantendo a colisão vertical (gravidade) intacta.

## 3. Comandos de Build
- O projeto foi compilado utilizando `make` após cada alteração para garantir a integridade do código.
- Binário resultante: `bin/Linux/main`.
