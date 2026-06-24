# Conversation Log — Trabalho Final Eduardo & Vitor

## 1. Lock-on Target Key
**Prompt:** qual a tecla usada para fazer lock-on-target ?
**Resposta:** `K` (definido em `src/main.cpp:5741`).

---

## 2. Cycle Lock-on Targets com Setas
**Prompt:** implemente um lockoncamera onde posso alterar entre os inimigos com a setinhas do teclado
**Resposta:**
- `include/enemies.h:155` — declaração `QueryNextLockOnEnemy()`
- `src/enemies.cpp:1502-1517` — implementação da busca do próximo/anterior inimigo
- `src/main.cpp:1443-1457` — função `CycleLockOnTarget()`
- `src/main.cpp:5750-5756` — key bindings: `RIGHT`/`LEFT` trocam alvo no modo LockOn

---

## 3. Lock-on sem inimigo → recentralizar câmera
**Prompt:** esse K tbm quando não tiver nenhum inimigo para fazer lock, para orientar a câmera com player-yaw
**Resposta:** `src/main.cpp:1425-1429` — ao pressionar `K` sem inimigos, `g_CameraYaw = g_PlayerYaw` e `g_CameraInitialized = false`

---

## 4. TAB — Equipar/Desquipar Estilingue
**Prompt:** se a pessoa apertar TAB, ela vai desquipar o escudo e espada e vai equipar o estilingue na mão direita e vai poder atirar
**Resposta:**
- `src/main.cpp:436-438` — globais `g_SlingshotAttachment`, `g_SlingshotObjectNames`, `g_SlingshotEquipped`
- `src/main.cpp:2940-2970` — carrega `assets/char/estilingue.obj`, cria attachment invisível na mão direita
- `src/main.cpp:1473-1492` — função `ToggleSlingshotEquip()`
- `src/main.cpp:5898-5899` — key binding `TAB`
- `src/main.cpp:5676,5687,3610` — tiro funciona com `g_SlingshotEquipped` (não só primeira pessoa)
- `src/main.cpp:1317-1333,1336-1354` — ao trocar de câmera, estilingue é desequipado automaticamente

---

## 5. Modo de Posicionamento do Estilingue (F2)
**Prompt:** o estilingue não tá bem posicionado, quero que coloque um modo para eu posicionar na mão
**Resposta:**
- `src/main.cpp:439` — global `g_SlingshotTuningMode`
- `src/main.cpp:5858-5910` — F2 toggles tuning mode. Controles: WASD = pos X/Z, Q/E = altura Y, Setas = rot X/Y, R/F = roll Z, SHIFT = rápido, ESPAÇO = print valores
- `src/main.cpp:3910-3913` — animação congela no modo tuning

---

## 6. Posição Final do Estilingue
**Prompt:** (usuário ajustou e mandou os valores via log)
**Resposta:** `src/main.cpp:2997-2998` — posição padrão hardcodada: `pos=(53, -63, -23) rot=(330, 790, 280)`

---

## 7. Print Periódico da Posição do Estilingue
**Prompt:** mas tu botou o printf no terminal da posição da weapon?
**Resposta:** `src/main.cpp:4093-4099` — adicionada linha do estilingue no print periódico `[WEAPON]`

---

## 8. Bloquear Ataque/Defesa com Estilingue
**Prompt:** o personagem quando tiver com estilingue equipado, ele não pode ter animação de ataque e nem de defesa
**Resposta:**
- `src/main.cpp:5992-5994` — E e Q bloqueados quando `g_SlingshotEquipped`
- `src/main.cpp:1487-1488` — `g_AttackPressed` e `g_DefendPressed` resetados ao equipar estilingue

---

## 9. Tiro em Terceira Pessoa + Mira
**Prompt:** quando tiver na terceira pessoa, a munição tem que sair do link em linha reta com base no PLAYER YAW e na primeira pessoa é com base em onde tá mirando (crie uma seta no meio da tela)
**Resposta:**
- `src/main.cpp:3628-3641` — terceira pessoa: tiro usa `g_PlayerYaw` como direção, spawna na frente do player. Primeira pessoa: usa câmera.
- `src/main.cpp:1714-1754` — função `DrawSlingshotReticle()` desenha crosshair no centro da tela
- `src/main.cpp:4377` — chamada `DrawSlingshotReticle()` antes de `glfwSwapBuffers`

---

## 10. Mira Só em Primeira Pessoa
**Prompt:** quero só se tiver em primeira pessoa e com o estilingue
**Resposta:** `src/main.cpp:1719` — condição `!g_SlingshotEquipped || !g_FirstPersonCamera`

---

## 11. COBWEB_FLOORHOLE Renderizando 2x
**Prompt:** cobweb_floohole está redenrizando duas vezes
**Resposta:** `src/main.cpp:3770` — adicionado `COBWEB_FLOORHOLE` ao skip do Path 1 (geometria estática), renderizado só pelo Path 2 (objects.json)

---

## 12. Cobweb Não Quebra
**Prompt:** e o buraco não tá se abrindo / NÃO QUEBRA NÃO QUEBRA EU PULEI TRI ALTO E NÃO QUEBRA
**Resposta:**
- `src/movement.cpp:316` — cobweb quebra ao pisar (`velY < 0.0f`), não precisa mais de velocidade alta (-7.0 → -5.0 → 0.0)
- `src/movement.cpp:333-375` — quebra integrada na resolução de colisão vertical (dentro do `if (!yFree)`), evita que `g_PlayerOnGround = true` antes do check
