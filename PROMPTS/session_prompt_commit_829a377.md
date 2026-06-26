# Conversa — Trabalho Final Eduardo & Vitor

## 1. Desbloquear estilingue ao abrir baú (via `objects.json`)

**Prompt:** "dentro do objects.json quando tiver um do tipo CHEST, ele deve habilitar alguma coisa, no primeiro bau deve habilitar o jogador de jogar com o estilingue"

**Resposta:** Implementado sistema onde baús do tipo `CHEST` podem ter um campo `item` no `objects.json` que define o que concedem ao serem abertos.

**Arquivos alterados:**

| Arquivo | Mudança |
|---|---|
| `src/main.cpp` | `SceneObjectInstance.item`, parse do campo `item` no JSON, `ChestInstance.item`, flag `g_SlingshotUnlocked`, guard no `ToggleSlingshotEquip()`, desbloqueio ao abrir baú |
| `include/collision.h` | Campo `std::string item` no `ChestInstance` |
| `assets/scenes/scene02/objects.json` | `"item": "SLINGSHOT"` adicionado ao baú |

**Fluxo:**
1. TAB antes do baú → "Você ainda não tem o estilingue!"
2. Abrir baú com ENTER → "Você encontrou o estilingue! Pressione TAB para equipá-lo."
3. TAB agora funciona para equipar/despejar estilingue.

**Extensível:** Basta adicionar `"item": "BOW"`, `"item": "BOOMERANG"` etc. em outros baús.

---

## 2. Animar estilingue saindo do baú

**Prompt:** "faça um animação do estilingue saindo do bau"

**Resposta:** Ao abrir baú com `item: "SLINGSHOT"`, modelo 3D do estilingue surge acima do baú com animação de:
- Flutuação para cima (~2.5 unidades) com ease-out
- Rotação de 3 voltas no eixo Y
- Tint dourado (1.0, 0.9, 0.2)
- Duração de 2 segundos

**Arquivo alterado:** `src/main.cpp` — struct `ChestItemAnim`, globais `g_SlingshotModelCenter` / `g_SlingshotModelScale`, update + render da animação.

---

## 3. Melhorar colisão de interação com o baú

**Prompt:** "a colisão da caixa tá meio ruim, preciso quase que entrar dentro do bau para funcionar, verifique isso ?"

**Resposta:** Alterada detecção de distância centro-a-centro (`dist < 2.0`) para interseção AABB com OBB expandido do jogador (1.5x half-extents) — mesmo método usado para portas. Muito mais tolerante.

**Arquivo alterado:** `src/main.cpp` — função de interação com baú.
