 eu quero que as verificações de input do movement.cpp vão para o keycallback da main. Faça 
   isso e mantenha a lógica correta                                                           
▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀

  Relocating Input Verification: 
  Researching input handling and movement logic to relocate input verification from
  movement.cpp to the keycallback in main.cpp.

  ✓  FindFiles  '**/movement.cpp' → Found 1 matching file(s)
  ✓  FindFiles  '**/main.cpp' → Found 2 matching file(s)
  ✓  FindFiles  '**/movement.cpp' → Found 1 matching file(s)
  ✓  ReadFile  trabalho_final_.../.../movement.cpp
  ✓  ReadFile  trabalho_final_edua.../.../main.cpp
   → Read lines 1-2000 of 2012 from trabalho_final_eduardo_vitor/src/main.cpp
  ✓  ReadFile  trabalho_final_.../.../movement.cpp
  ✓  FindFiles  '**/globals.h' → Found 8 matching file(s)
  ✓  ReadFile  trabalho_final_edu.../.../globals.h
  ✓  FindFiles  '**/globals.cpp' → Found 1 matching file(s)
  ✓  ReadFile  trabalho_final_e.../.../globals.cpp
  Updating Globals for Input States: Updating globals to include input state flags.

  ✓  Edit  globals.h → Accepted (+8, -1)

      10   extern bool g_PlayerCubeColliding;
      11   extern std::vector<CollisionShape> g_ScenarioCollisionShapes;
      12   extern float g_PlayerVerticalVelocity;
      13 - extern bool  g_PlayerOnGround;
      13 + extern bool  g_PlayerOnGround;
      14 + 
      15 + // Flags de input para movimento
      16 + extern bool g_WPressed;
      17 + extern bool g_APressed;
      18 + extern bool g_SPressed;
      19 + extern bool g_DPressed;
      20 + extern bool g_SpacePressed;

  ✓  Edit  globals.cpp → Accepted (+7, -1)

       9   bool g_PlayerCubeColliding = false;
      10   std::vector<CollisionShape> g_ScenarioCollisionShapes;
      11   float g_PlayerVerticalVelocity = 0.0f;
      12 - bool  g_PlayerOnGround = false;
      12 + bool  g_PlayerOnGround = false;
      13 + 
      14 + bool g_WPressed = false;
      15 + bool g_APressed = false;
      16 + bool g_SPressed = false;
      17 + bool g_DPressed = false;
      18 + bool g_SpacePressed = false;

  Updating KeyCallback in main.cpp: Implementing input state updates in KeyCallback.

  ✓  Edit  main.cpp → Accepted (+7, -0)

      1689           fprintf(stdout, "Shaders recarregados!\n");
      1690           fflush(stdout);
      1691       }
      1692 + 
      1693 +     // Atualiza flags de input para movimento
      1694 +     if (key == GLFW_KEY_W) g_WPressed = (action != GLFW_RELEASE);
      1695 +     if (key == GLFW_KEY_A) g_APressed = (action != GLFW_RELEASE);
      1696 +     if (key == GLFW_KEY_S) g_SPressed = (action != GLFW_RELEASE);
      1697 +     if (key == GLFW_KEY_D) g_DPressed = (action != GLFW_RELEASE);
      1698 +     if (key == GLFW_KEY_SPACE) g_SpacePressed = (action != GLFW_RELEASE);
      1699   }
      1700
      1701   // Definimos o callback para impressão de erros da GLFW no terminal

  ✓  ReadFile  trabalho_final_ed.../.../movement.h
  Refactoring movement.cpp: Refactoring movement logic to use global input flags.

  ✓  Edit  movement.cpp → Accepted (+5, -5)

      19   const float turn_speed = 2.1f;
      20
      21   // rotation
      22 - if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      22 + if (g_APressed)
      23       g_PlayerYaw -= turn_speed * delta_time;
      24
      25 - if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      25 + if (g_DPressed)
      26       g_PlayerYaw += turn_speed * delta_time;
      27
      28   const glm::vec4 player_back(
      33
      34   float move_input = 0.0f;
      35
      36 - if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      36 + if (g_WPressed)
      37       move_input += 1.0f;
      38
      39 - if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      39 + if (g_SPressed)
      40       move_input -= 1.0f;
      41
      42   intended_move =
      46   // aplicar gravidade
      47   g_PlayerVerticalVelocity += gravity * delta_time;
      48
      49 - if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && g_PlayerOnGround)
      49 + if (g_SpacePressed && g_PlayerOnGround)
      50   {
      51       g_PlayerVerticalVelocity = jump_speed;
      52       g_PlayerOnGround = false;

  ✓  ReadFile  trabalho_final_.../.../movement.cpp
  ✓  SearchText  'glfwGetKey' within ./ → Found 12 matches
╭────────────────────────────────────────────────────────────────────────────────────────╮
│ ✓  Shell grep "glfwGetKey" trabalho_final_eduardo_vitor/src/main.cpp                   │
│                                                                                        │
│                                                                                        │
╰────────────────────────────────────────────────────────────────────────────────────────╯
╭────────────────────────────────────────────────────────────────────────────────────────╮
│ ✓  Shell grep "glfwGetKey" trabalho_final_eduardo_vitor/src/movement.cpp               │
│                                                                                        │
│                                                                                        │
╰────────────────────────────────────────────────────────────────────────────────────────╯

✦ As verificações de input foram movidas do arquivo movement.cpp para o KeyCallback no arquivo
  main.cpp. Para manter a lógica de movimento contínuo e o comportamento do pulo, utilizei um
  sistema de flags globais que são atualizadas por eventos no KeyCallback e lidas a cada
  quadro na função de movimento.

  Alterações realizadas:

   1. include/globals.h: Foram declaradas novas variáveis globais para rastrear o estado das
      teclas (W, A, S, D e Space).
   2. src/globals.cpp: As variáveis foram inicializadas como false.
   3. src/main.cpp: A função KeyCallback foi atualizada para alterar o estado das flags sempre
      que uma tecla é pressionada (GLFW_PRESS ou GLFW_REPEAT) ou solta (GLFW_RELEASE).
   4. src/movement.cpp: A função UpdatePlayerMovement agora utiliza essas flags globais em vez
      de chamar glfwGetKey, eliminando o polling direto de input dentro da lógica de física.

  Essa abordagem garante que a lógica permaneça correta (movimento fluido enquanto a tecla é
  segurada) e desacopla a verificação de input do processamento de movimento, conforme
  solicitado.


  Input Verification Relocated: 
  Relocated input verification from movement.cpp to main.cpp using global state flags.

▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
  cria um sistema de debug onde desenha hitbox do personagem e a das ledder
▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀

  Debug Hitbox System: 
  Creating a debug visualization system for character and ladder hitboxes using wireframe
  AABB rendering.

  ✓  ReadFile  globals.h → Found existing collision/movement globals
  ✓  ReadFile  collision.h → Found CollisionShape struct with LADDER, VINES types
  ✓  ReadFile  main.cpp → Read full rendering pipeline and shader setup
  ✓  ReadFile  shader_fragment.glsl → Read fragment shader with object_id system
  ✓  ReadFile  movement.cpp → Read ladder/vine collision detection logic

  Updating Globals for Debug: Adding debug hitbox toggle flag.

  ✓  Edit  globals.h → Accepted (+3, -1)

      20 + extern bool g_CollidedWithAVine;
      21 + extern bool g_CollidedWithALadder;
      22 + 
      23 + // Debug drawing
      24 + extern bool g_ShowDebugHitboxes;

  ✓  Edit  globals.cpp → Accepted (+2, -0)

      19 + bool g_CollidedWithAVine = false;
      20 + bool g_CollidedWithALadder = false;
      21 + 
      22 + bool g_ShowDebugHitboxes = false;

  Building Debug Wireframe Cube: Creating wireframe cube mesh for AABB visualization.

  ✓  Edit  main.cpp → Accepted (+65, -0)

      // Debug drawing system
      static GLuint g_DebugWireframeVAO = 0;
      static GLuint g_DebugWireframeVBO = 0;
      static GLuint g_DebugWireframeEBO = 0;
      static int g_DebugWireframeIndexCount = 0;

      void BuildDebugWireframeCube();
      void DrawDebugAABB(const glm::vec4 &center, const glm::vec4 &half_extents, const glm::vec4 &color);

      // BuildDebugWireframeCube: 8 vertices + 12 edges (24 indices for GL_LINES)
      // DrawDebugAABB: Translates/scales unit cube to match AABB, draws wireframe

  Adding Debug Object ID: Setting up DEBUG_CUBE constant and shader uniform.

  ✓  Edit  main.cpp → Accepted (+1, -0)

      const int OBJECT_ID_DEBUG_CUBE = 5;

  ✓  Edit  shader_fragment.glsl → Accepted (+8, -2)

      #define DEBUG_CUBE 5
      uniform int cube_colliding;
      uniform vec4 debug_color;
      ...
      else if ( object_id == DEBUG_CUBE )
      {
          Kd0 = debug_color.rgb;
          alpha = debug_color.a;
      }

  Initializing Debug System: Building wireframe cube after shader load.

  ✓  Edit  main.cpp → Accepted (+3, -0)

      LoadShadersFromFiles();
      + // Build debug wireframe cube for hitbox visualization
      + BuildDebugWireframeCube();

  Adding Debug Render Loop: Drawing hitboxes when debug mode is enabled.

  ✓  Edit  main.cpp → Accepted (+45, -0)

      // Debug drawing: render hitboxes if enabled
      if (g_ShowDebugHitboxes)
      {
          glDisable(GL_DEPTH_TEST);
          glDisable(GL_CULL_FACE);

          // Draw player hitbox (green wireframe)
          glm::vec4 player_color(0.0f, 1.0f, 0.0f, 1.0f);
          DrawDebugAABB(g_PlayerCubePosition, g_PlayerCubeHalfExtents, player_color);

          // Draw ladder hitboxes (yellow wireframe)
          for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
          {
              if (g_ScenarioCollisionShapes[i].type == CollisionShapeType::LADDER)
              {
                  glm::vec4 ladder_center = (min + max) * 0.5f;
                  glm::vec4 ladder_half = (max - min) * 0.5f;
                  glm::vec4 ladder_color(1.0f, 1.0f, 0.0f, 1.0f);
                  DrawDebugAABB(ladder_center, ladder_half, ladder_color);
              }
          }

          // Draw vine hitboxes (magenta wireframe)
          // ... similar loop for VINES type
      }

  Adding F1 Key Toggle: Enabling debug mode toggle.

  ✓  Edit  main.cpp → Accepted (+7, -0)

      // F1 toggles debug hitbox visualization
      if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
      {
          g_ShowDebugHitboxes = !g_ShowDebugHitboxes;
          fprintf(stdout, "Debug hitboxes: %s\n", g_ShowDebugHitboxes ? "ON" : "OFF");
      }

  ✓  Build successful → Compiled and ran without errors
  ✓  Runtime verified → Debug system initialized correctly

  ✦ Implementado sistema completo de debug para visualização de hitboxes. O sistema permite
    visualizar as caixas de colisão do personagem, escadas e videiras como wireframes
    coloridos sobrepostos à cena.

  Alterações realizadas:

   1. include/globals.h: Adicionada flag g_ShowDebugHitboxes para controlar o modo debug.
   2. src/globals.cpp: Flag inicializada como false (desligada por padrão).
   3. src/main.cpp: 
      - Criada malha wireframe unitária (BuildDebugWireframeCube) com 8 vértices e 12 arestas.
      - Função DrawDebugAABB que desenha qualquer AABB como cubo wireframe colorido.
      - Inicialização da malha wireframe após carregamento dos shaders.
      - Loop de renderização debug que desenha:
        * Verde: Hitbox do personagem (g_PlayerCubePosition + g_PlayerCubeHalfExtents)
        * Amarelo: Hitbox de escadas (LADDER)
        * Magenta: Hitbox de videiras (VINES)
      - Tecla F1 para ligar/desligar o modo debug.
   4. src/shader_fragment.glsl: 
      - Definição DEBUG_CUBE = 5.
      - Uniform debug_color para definir a cor do wireframe.

  Como usar:

   - Pressione F1 para ligar/desligar a visualização das hitboxes.
   - As hitboxes são desenhadas como wireframes sobrepostos aos objetos.
   - Profundidade e culling são desabilitados durante o debug para melhor visibilidade.


  Debug Hitbox System Created: 
  Debug visualization system for character and ladder/vine hitboxes implemented with wireframe AABB rendering.
