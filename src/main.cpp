// Headers de C
#include <cmath>
#include <cstdio>
#include <cstdlib>

// Headers abaixo são específicos de C++
#include <iostream>
#include <set>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>  // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h> // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"
#include "collision.h"
#include "types.h"
#include "globals.h"
#include "enemies.h"
#include "movement.h"
#include "AssimpModelLoader.h"

GLuint LoadTextureImage(const char *filename); // Função que carrega imagens de textura
void SplitShapesByMaterial(std::vector<tinyobj::shape_t> *shapes);

// Estrutura que representa um modelo geométrico carregado a partir de um
// arquivo ".obj".
struct ObjModel
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::vector<GLuint> material_texture_ids;

    // Este construtor lê o modelo de um arquivo utilizando a biblioteca tinyobjloader.
    // Veja: https://github.com/syoyo/tinyobjloader
    ObjModel(const char *filename, const char *basepath = NULL, bool triangulate = true)
    {
        printf("Carregando objetos do arquivo \"%s\"...\n", filename);

        // Se basepath == NULL, então setamos basepath como o dirname do
        // filename, para que os arquivos MTL sejam corretamente carregados caso
        // estejam no mesmo diretório dos arquivos OBJ.
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL)
        {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos)
            {
                dirname = fullpath.substr(0, i + 1);
                basepath = dirname.c_str();
            }
        }

        std::string warn;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        SplitShapesByMaterial(&shapes);

        for (size_t shape = 0; shape < shapes.size(); ++shape)
        {
            if (shapes[shape].name.empty())
            {
                fprintf(stderr,
                        "*********************************************\n"
                        "Erro: Objeto sem nome dentro do arquivo '%s'.\n"
                        "Veja https://www.inf.ufrgs.br/~eslgastal/fcg-faq-etc.html#Modelos-3D-no-formato-OBJ .\n"
                        "*********************************************\n",
                        filename);
                throw std::runtime_error("Objeto sem nome.");
            }
            printf("- Objeto '%s'\n", shapes[shape].name.c_str());
        }

        printf("Carregando as texturas...\n");
        for (size_t i = 0; i < materials.size(); i++)
        {
            if (!materials[i].diffuse_texname.empty())
            {
                std::string textpath = std::string(basepath) + materials[i].diffuse_texname;
                GLuint texture_id = LoadTextureImage(textpath.c_str());
                printf("[ModelLoad] material='%s' diffuse='%s' texture_id=%u\n",
                       materials[i].name.c_str(),
                       textpath.c_str(),
                       texture_id);
                material_texture_ids.push_back(texture_id);
            }
            else
            {
                printf("[ModelLoad] material='%s' diffuse='' texture_id=0\n",
                       materials[i].name.c_str());
                material_texture_ids.push_back(0);
            }
        }

        printf("OK.\n");
    }
};

// Declaração de funções utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4 &M);

// Declaração de várias funções utilizadas em main().  Essas estão definidas
// logo após a definição de main() neste arquivo.
void BuildTrianglesAndAddToVirtualScene(ObjModel *, GLuint default_texture_id = 0); // Constrói representação de um ObjModel como malha de triângulos para renderização
void BuildTrianglesFromAssimpAndAddToVirtualScene(AssimpModelLoader &loader); // Constrói representação de um modelo Assimp como malha de triângulos para renderização
void ComputeNormals(ObjModel *model);                                         // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles();                                                  // Carrega os shaders de vértice e fragmento, criando um programa de GPU
void DrawVirtualObject(const char *object_name);                              // Desenha um objeto armazenado em g_VirtualScene
GLuint LoadShader_Vertex(const char *filename);                               // Carrega um vertex shader
GLuint LoadShader_Fragment(const char *filename);                             // Carrega um fragment shader
void LoadShader(const char *filename, GLuint shader_id);                      // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id);  // Cria um programa de GPU
void PrintObjModelInfo(ObjModel *);                                           // Função para debugging
void BuildUnitCubeAndAddToVirtualScene(const char *object_name);              // Constrói cubo unitário para teste de colisão
void BuildCollisionDataFromObjModel(ObjModel *model, glm::mat4 model_matrix); // Constrói dados de colisão para o cenário
void BuildCollisionDataIntoVector(ObjModel *model, glm::mat4 model_matrix,
    std::vector<CollisionShape> &out_shapes,
    glm::vec4 &out_bbox_min, glm::vec4 &out_bbox_max); // Variante que armazena em vetor externo
void ComputeObjBounds(ObjModel *model, glm::vec4 &bbox_min, glm::vec4 &bbox_max);
bool FindCameraObstructionDistance(const glm::vec4 &pivot_position, const glm::vec4 &desired_camera_position, float &hit_distance);

// Declaração de funções auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funções estão definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow *window);
float TextRendering_CharWidth(GLFWwindow *window);
void TextRendering_PrintString(GLFWwindow *window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrix(GLFWwindow *window, glm::mat4 M, float x, float y, float scale = 1.0f);
void TextRendering_PrintVector(GLFWwindow *window, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProduct(GLFWwindow *window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductMoreDigits(GLFWwindow *window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductDivW(GLFWwindow *window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowModelViewProjection(GLFWwindow *window, glm::mat4 projection, glm::mat4 view, glm::mat4 model, glm::vec4 p_model);
void TextRendering_ShowEulerAngles(GLFWwindow *window);
void TextRendering_ShowProjection(GLFWwindow *window);
void TextRendering_ShowFramesPerSecond(GLFWwindow *window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow *window, int width, int height);
void ErrorCallback(int error, const char *description);
void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow *window, double xpos, double ypos);
void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject
{
    std::string name;              // Nome do objeto
    size_t first_index;            // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    size_t num_indices;            // Número de índices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum rendering_mode;         // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint vertex_array_object_id; // ID do VAO onde estão armazenados os atributos do modelo
    glm::vec4 bbox_min;            // Axis-Aligned Bounding Box do objeto
    glm::vec4 bbox_max;
    GLuint texture_id; // ID da textura a ser utilizada para renderizar o objeto, ou 0 para não utilizar textura
    GLuint sampler_id; // ID do sampler a ser utilizado para a textura
};

// Abaixo definimos variáveis globais utilizadas em várias funções do código.

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário
// (map).  Veja dentro da função BuildTrianglesAndAddToVirtualScene() como que são incluídos
// objetos dentro da variável g_VirtualScene, e veja na função main() como
// estes são acessados.
std::map<std::string, SceneObject> g_VirtualScene;
static std::map<std::string, GLuint> g_TextureCache;
static std::map<std::string, GLuint> g_SamplerCache;

// Pilha que guardará as matrizes de modelagem.
std::stack<glm::mat4> g_MatrixStack;
std::vector<std::string> g_ScenarioObjectNames;

// Estrutura que representa uma parte do mapa (scene00 a scene11).
struct ScenePart
{
    std::string name;                               // "scene00", "scene01", ...
    std::vector<std::string> render_object_names;   // Nomes dos objetos no g_VirtualScene
    std::vector<CollisionShape> collision_shapes;    // Shapes de colisao desta parte
    glm::vec4 bbox_min, bbox_max;                   // AABB em world-space
    std::vector<size_t> adjacent_indices;            // Indices das cenas adjacentes
};

std::vector<ScenePart> g_SceneParts;
std::vector<size_t> g_CurrentActiveSceneIndices;
GLuint g_DefaultGrayTextureID = 0;

// Declaração desta variável foram movidas para globals.h
// static std::vector<CollisionShape> g_ScenarioCollisionShapes;

glm::vec4 g_ScenarioBoundsMin = glm::vec4(+std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), 1.0f);
glm::vec4 g_ScenarioBoundsMax = glm::vec4(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);

glm::mat4 g_ScenarioModelMatrix = Matrix_Identity();
const char *g_SceneMapPath = "../../assets/scenes/scene00/map.obj";
const char *g_SceneCollisionPath = "../../assets/scenes/scene00/collision.obj";

static const glm::vec4 g_HardcodedTestSpawnPosition(15.0f, 0.75f, -0.26f, 1.0f);

// Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
float g_ScreenRatio = 1.0f;

// Ângulos de Euler que controlam a rotação de um dos cubos da cena virtual
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;

// "g_LeftMouseButtonPressed = true" se o usuário está com o botão esquerdo do mouse
// pressionado no momento atual. Veja função MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false;  // Análogo para botão direito do mouse
bool g_MiddleMouseButtonPressed = false; // Análogo para botão do meio do mouse

// Variáveis que definem a câmera em coordenadas esféricas, controladas pelo
// usuário através do mouse (veja função CursorPosCallback()). A posição
// efetiva da câmera é calculada dentro da função main(), dentro do loop de
// renderização.
float g_CameraTheta = 0.0f;     // Ângulo no plano ZX em relação ao eixo Z
float g_CameraPhi = 0.0f;       // Ângulo em relação ao eixo Y
float g_CameraDistance = 14.0f; // Distância da câmera para a origem

// Variáveis que controlam a
bool g_FirstPersonCamera = false;
glm::vec4 g_PositionCameraFirstPerson = glm::vec4(0.0f, 0.0f, 0.08f, 0.0f);

// Variáveis que controlam a câmera third-person estilo Zelda-like:
bool g_ThirdPersonCamera = true;
float g_ThirdPersonCameraDistance = 3.2f;
float g_ThirdPersonCameraHeight = 1.4f;
float g_ThirdPersonLookAtHeight = 0.4f;
glm::vec4 g_CameraCollisionHalfExtents(0.16f, 0.16f, 0.16f, 0.0f);
constexpr float CAMERA_WALL_PADDING = 0.15f;
constexpr float CAMERA_IN_SPEED = 20.0f;
constexpr float CAMERA_OUT_SPEED = 6.0f;
constexpr float CAMERA_MIN_DISTANCE = 0.5f;
float g_CameraYaw = 0.0f;
float g_CurrentThirdPersonCameraDistance = g_ThirdPersonCameraDistance;
bool g_CameraInitialized = false;
float g_CameraYawFollowSpeed = 4.0f;

// Variáveis que controlam rotação do antebraço
float g_ForearmAngleZ = 0.0f;
float g_ForearmAngleX = 0.0f;

// Variáveis que controlam translação do torso
float g_TorsoPositionX = 0.0f;
float g_TorsoPositionY = 0.0f;

// Variável que controla o tipo de projeção utilizada: perspectiva ou ortográfica.
bool g_UsePerspectiveProjection = true;

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = true;

// Variáveis que definem um programa de GPU (shaders). Veja função LoadShadersFromFiles().
GLuint g_GpuProgramID = 0;
GLint g_model_uniform;
GLint g_model_normal_matrix_uniform;
GLint g_view_uniform;
GLint g_projection_uniform;
GLint g_object_id_uniform;
GLint g_bbox_min_uniform;
GLint g_bbox_max_uniform;
GLint g_cube_colliding_uniform;
GLint g_object_tint_uniform;

// Uniforms para animação e texturas do jogador
GLint g_bones_uniform[100];
GLint g_use_animation_uniform;
GLint g_player_texture_uniform;
GLint g_has_player_texture_uniform;
GLint g_material_diffuse_uniform;
GLint g_material_specular_uniform;
GLint g_material_ambient_uniform;
GLint g_material_shininess_uniform;

// Número de texturas carregadas pela função LoadTextureImage()
GLuint g_NumLoadedTextures = 0;

const int OBJECT_ID_SCENARIO = 3;
const int OBJECT_ID_PLAYER_CUBE = 4;
const int OBJECT_ID_DEBUG_CUBE = 6;
const int OBJECT_ID_SPHERE = 0;
const int OBJECT_ID_PROJECTILE = 5;
const int OBJECT_ID_PLAYER = 7;
const int OBJECT_ID_ENEMY = 8;

// Variáveis para o modelo do jogador e animação
AssimpModelLoader g_PlayerModelLoader;
float g_PlayerAnimationTime = 0.0f;
float g_AnimationSpeedMultiplier = 1.0f;
static int g_AnimDebugFrameCounter = 0;
static const int g_AnimDebugPrintInterval = 120;
std::vector<glm::mat4> g_PlayerAnimationTransforms;
GLuint g_PlayerTextureID = 0;
bool g_HasPlayerTexture = false;
std::vector<GLuint> g_PlayerMeshTextureIDs; // Textura por mesh do jogador

// Animações

glm::vec4 camera_position_c;
glm::vec4 camera_lookat_l;
glm::vec4 camera_view_vector;
glm::vec4 camera_up_vector;

// Debug drawing system
static GLuint g_DebugWireframeVAO = 0;
static GLuint g_DebugWireframeVBO = 0;
static GLuint g_DebugWireframeEBO = 0;
static int g_DebugWireframeIndexCount = 0;
static GLuint g_DebugArrowVAO = 0;
static GLuint g_DebugArrowVBO = 0;
static int g_DebugArrowVertexCount = 0;

void BuildDebugWireframeCube();
void DrawDebugAABB(const glm::vec4 &center, const glm::vec4 &half_extents, const glm::vec4 &color);
void BuildDebugArrowVAO();
void DrawDebugArrow(const glm::vec4 &origin, const glm::vec4 &direction, const glm::vec4 &color);

static float ComputeStrideLength(AssimpModelLoader &loader, int animIdx,
                                 float modelScale, const char *animLabel)
{
    if (animIdx < 0 || animIdx >= (int)loader.GetAnimations().size()) {
        printf("[STRIDE] %s: invalid animIdx=%d, skipping\n", animLabel, animIdx);
        return 0.0f;
    }

    float cycleSec = loader.GetAnimationDurationSeconds(animIdx);
    if (cycleSec <= 0.0f) {
        printf("[STRIDE] %s: duration=%.4f, skipping\n", animLabel, cycleSec);
        return 0.0f;
    }

    const char *hipsBone = "mixamorig:Hips";
    if (!loader.HasBone(hipsBone)) {
        printf("[STRIDE] %s: no Hips bone, skipping\n", animLabel);
        return 0.0f;
    }

    // Amostra deslocamento do Hips (root motion) ao longo de 1 ciclo
    const int numSamples = 100;
    float minZ = 1e10f, maxZ = -1e10f;
    float minX = 1e10f, maxX = -1e10f;

    for (int s = 0; s < numSamples; s++) {
        float t = ((float)s / (float)(numSamples - 1)) * cycleSec;
        loader.SampleBoneHierarchy(animIdx, t);
        glm::vec3 pos = loader.GetLastSampledBonePos(hipsBone);
        if (pos.z < minZ) minZ = pos.z;
        if (pos.z > maxZ) maxZ = pos.z;
        if (pos.x < minX) minX = pos.x;
        if (pos.x > maxX) maxX = pos.x;
    }

    float strideX = maxX - minX;
    float strideZ = maxZ - minZ;
    float strideModel = sqrtf(strideX * strideX + strideZ * strideZ);

    // Se Hips quase não se desloca (in-place), mede pelo pé como fallback
    if (strideModel < 0.01f) {
        const char *footBone = "mixamorig:LeftFoot";
        if (!loader.HasBone(footBone))
            footBone = "mixamorig:RightFoot";
        if (loader.HasBone(footBone)) {
            minX = minZ = 1e10f; maxX = maxZ = -1e10f;
            for (int s = 0; s < numSamples; s++) {
                float t = ((float)s / (float)(numSamples - 1)) * cycleSec;
                loader.SampleBoneHierarchy(animIdx, t);
                glm::vec3 pos = loader.GetLastSampledBonePos(footBone);
                if (pos.z < minZ) minZ = pos.z;
                if (pos.z > maxZ) maxZ = pos.z;
                if (pos.x < minX) minX = pos.x;
                if (pos.x > maxX) maxX = pos.x;
            }
            strideX = maxX - minX;
            strideZ = maxZ - minZ;
            strideModel = sqrtf(strideX * strideX + strideZ * strideZ);
            hipsBone = footBone;
            printf("[STRIDE] %s (in-place, using %s instead of Hips)\n", animLabel, footBone);
        }
    }

    float strideWorld = strideModel * modelScale;
    float speed = strideWorld / cycleSec;

    printf("[STRIDE] %s animIdx=%d bone='%s' cycleDur=%.4fs\n",
           animLabel, animIdx, hipsBone, cycleSec);
    printf("[STRIDE] %s modelStride=%.4f (X=%.4f Z=%.4f) worldStride=%.4f speed=%.4f u/s\n",
           animLabel, strideModel, strideX, strideZ, strideWorld, speed);

    printf("[STRIDE] %s %sZ samples (every 10%%):", animLabel, hipsBone);
    for (int s = 0; s < numSamples; s += 10) {
        float t = ((float)s / (float)(numSamples - 1)) * cycleSec;
        loader.SampleBoneHierarchy(animIdx, t);
        glm::vec3 pos = loader.GetLastSampledBonePos(hipsBone);
        printf(" %.1f", pos.z);
    }
    printf("\n");

    return speed;
}

struct SlingshotState
{
    bool is_charging;
    bool fire_requested;
    float charge_time_seconds;
    float queued_charge_ratio;
    float max_charge_time_seconds;
    float shot_cooldown_seconds;
    float shot_cooldown_timer;
};

struct ProjectileState
{
    bool is_active;
    glm::vec4 position;
    glm::vec4 direction;
    float speed;
    float radius;
    float lifetime_seconds;
    float max_lifetime_seconds;
};

enum class ProjectileCollisionTarget
{
    NONE,
    SCENARIO,
    ENEMY,
    INTERACTIVE_OBJECT
};

struct ProjectileCollisionResult
{
    ProjectileCollisionTarget target;
    CollisionShapeType scenario_shape_type;
    int enemy_index;
    int object_index;
};

SlingshotState g_SlingshotState = {false, false, 0.0f, 0.0f, 1.0f, 0.28f, 0.0f};
ProjectileState g_SlingshotProjectile = {
    false,
    glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
    glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
    0.0f,
    0.08f,
    0.0f,
    3.0f};

RenderModelInfo g_DekuBabaRenderInfo = {false, {}, glm::vec4(0.0f), 1.0f};
RenderModelInfo g_DekuScrubRenderInfo = {false, {}, glm::vec4(0.0f), 1.0f};
RenderModelInfo g_DekuScrubPlantRenderInfo = {false, {}, glm::vec4(0.0f), 1.0f};
RenderModelInfo g_DekuScrubProjectileRenderInfo = {false, {}, glm::vec4(0.0f), 1.0f};
RenderModelInfo g_SpiderRenderInfo = {false, {}, glm::vec4(0.0f), 1.0f};
RenderModelInfo g_QueenGohmaRenderInfo = {false, {}, glm::vec4(0.0f), 1.0f};
RenderModelInfo g_SphereRenderInfo = {false, {}, glm::vec4(0.0f), 1.0f};

static RenderModelInfo BuildRenderModelInfo(ObjModel &model, float desired_max_dimension)
{
    RenderModelInfo info = {true, {}, glm::vec4(0.0f), 1.0f};
    glm::vec4 bbox_min, bbox_max;
    ComputeObjBounds(&model, bbox_min, bbox_max);
    info.center = (bbox_min + bbox_max) * 0.5f;

    const glm::vec4 size = bbox_max - bbox_min;
    const float max_dimension = std::max(size.x, std::max(size.y, size.z));
    if (max_dimension > 1e-6f)
        info.base_scale = desired_max_dimension / max_dimension;

    info.object_names.clear();
    for (size_t i = 0; i < model.shapes.size(); ++i)
    {
        const std::string &shape_name = model.shapes[i].name;
        for (auto &[name, obj] : g_VirtualScene)
        {
            if (name.compare(0, shape_name.size(), shape_name) == 0 &&
                (name.size() == shape_name.size() ||
                 (name.size() > shape_name.size() && name[shape_name.size()] == '_')))
                info.object_names.push_back(name);
        }
    }

    return info;
}

glm::mat4 GetScenarioModelMatrix()
{
    return g_ScenarioModelMatrix;
}

void SplitShapesByMaterial(std::vector<tinyobj::shape_t> *shapes)
{
    std::vector<tinyobj::shape_t> split_shapes;
    split_shapes.reserve(shapes->size());

    for (const tinyobj::shape_t &shape : *shapes)
    {
        const tinyobj::mesh_t &mesh = shape.mesh;
        const size_t face_count = mesh.num_face_vertices.size();

        if (face_count == 0 || mesh.material_ids.empty())
        {
            split_shapes.push_back(shape);
            continue;
        }

        size_t face_begin = 0;
        size_t index_begin = 0;
        int block_index = 0;

        while (face_begin < face_count)
        {
            const int material_id = mesh.material_ids[face_begin];
            size_t face_end = face_begin;
            size_t index_end = index_begin;

            while (face_end < face_count && mesh.material_ids[face_end] == material_id)
            {
                index_end += mesh.num_face_vertices[face_end];
                ++face_end;
            }

            tinyobj::shape_t split_shape;
            split_shape.name = shape.name;
            if (face_begin != 0 || face_end != face_count)
                split_shape.name += "#mat" + std::to_string(material_id) + "_" + std::to_string(block_index);

            split_shape.mesh.indices.insert(
                split_shape.mesh.indices.end(),
                mesh.indices.begin() + static_cast<std::ptrdiff_t>(index_begin),
                mesh.indices.begin() + static_cast<std::ptrdiff_t>(index_end));
            split_shape.mesh.num_face_vertices.insert(
                split_shape.mesh.num_face_vertices.end(),
                mesh.num_face_vertices.begin() + static_cast<std::ptrdiff_t>(face_begin),
                mesh.num_face_vertices.begin() + static_cast<std::ptrdiff_t>(face_end));
            split_shape.mesh.material_ids.insert(
                split_shape.mesh.material_ids.end(),
                mesh.material_ids.begin() + static_cast<std::ptrdiff_t>(face_begin),
                mesh.material_ids.begin() + static_cast<std::ptrdiff_t>(face_end));

            if (!mesh.smoothing_group_ids.empty())
            {
                split_shape.mesh.smoothing_group_ids.insert(
                    split_shape.mesh.smoothing_group_ids.end(),
                    mesh.smoothing_group_ids.begin() + static_cast<std::ptrdiff_t>(face_begin),
                    mesh.smoothing_group_ids.begin() + static_cast<std::ptrdiff_t>(face_end));
            }

            split_shapes.push_back(std::move(split_shape));
            face_begin = face_end;
            index_begin = index_end;
            ++block_index;
        }
    }

    *shapes = std::move(split_shapes);
}

// Debug drawing: build a unit wireframe cube centered at origin
void BuildDebugWireframeCube()
{
    // 8 vertices of a unit cube
    const float vertices[] = {
        -0.5f, -0.5f, -0.5f,  // 0
        +0.5f, -0.5f, -0.5f,  // 1
        +0.5f, +0.5f, -0.5f,  // 2
        -0.5f, +0.5f, -0.5f,  // 3
        -0.5f, -0.5f, +0.5f,  // 4
        +0.5f, -0.5f, +0.5f,  // 5
        +0.5f, +0.5f, +0.5f,  // 6
        -0.5f, +0.5f, +0.5f,  // 7
    };

    // 12 edges (24 indices for GL_LINES)
    const GLuint indices[] = {
        0, 1,  1, 2,  2, 3,  3, 0,  // back face
        4, 5,  5, 6,  6, 7,  7, 4,  // front face
        0, 4,  1, 5,  2, 6,  3, 7,  // connecting edges
    };

    g_DebugWireframeIndexCount = 24;

    glGenVertexArrays(1, &g_DebugWireframeVAO);
    glBindVertexArray(g_DebugWireframeVAO);

    glGenBuffers(1, &g_DebugWireframeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, g_DebugWireframeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &g_DebugWireframeEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_DebugWireframeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Debug drawing: draw an AABB as a wireframe cube
void DrawDebugAABB(const glm::vec4 &center, const glm::vec4 &half_extents, const glm::vec4 &color)
{
    if (g_DebugWireframeVAO == 0)
        return;

    // Compute model matrix: translate to center, scale by 2*half_extents
    glm::mat4 model = Matrix_Translate(center.x, center.y, center.z) *
                      Matrix_Scale(half_extents.x * 2.0f, half_extents.y * 2.0f, half_extents.z * 2.0f);

    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(g_object_id_uniform, OBJECT_ID_DEBUG_CUBE); // Use DEBUG_CUBE for debug color

    // Set color uniform if it exists, otherwise use fixed function
    GLint color_loc = glGetUniformLocation(g_GpuProgramID, "debug_color");
    if (color_loc >= 0)
        glUniform4f(color_loc, color.x, color.y, color.z, color.w);

    // Draw wireframe
    glDisable(GL_CULL_FACE);
    glLineWidth(2.0f);
    glBindVertexArray(g_DebugWireframeVAO);
    glDrawElements(GL_LINES, g_DebugWireframeIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glLineWidth(1.0f);
    glEnable(GL_CULL_FACE);
}

void DrawDebugOBB(const CollisionOBB &obb, const glm::vec4 &color)
{
    if (g_DebugWireframeVAO == 0)
        return;

    glm::mat4 model = Matrix_Translate(obb.center.x, obb.center.y, obb.center.z) *
                      Matrix_Rotate_Y(-obb.yaw) *
                      Matrix_Scale(obb.half_extents.x * 2.0f, obb.half_extents.y * 2.0f, obb.half_extents.z * 2.0f);

    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(g_object_id_uniform, OBJECT_ID_DEBUG_CUBE);

    GLint color_loc = glGetUniformLocation(g_GpuProgramID, "debug_color");
    if (color_loc >= 0)
        glUniform4f(color_loc, color.x, color.y, color.z, color.w);

    glDisable(GL_CULL_FACE);
    glLineWidth(2.0f);
    glBindVertexArray(g_DebugWireframeVAO);
    glDrawElements(GL_LINES, g_DebugWireframeIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glLineWidth(1.0f);
    glEnable(GL_CULL_FACE);
}

void BuildDebugArrowVAO()
{
    const float vertices[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.85f, 0.05f, 0.05f,
        1.0f, 0.0f, 0.0f,
        0.85f, -0.05f, 0.05f,
        1.0f, 0.0f, 0.0f,
        0.85f, -0.05f, -0.05f,
        1.0f, 0.0f, 0.0f,
        0.85f, 0.05f, -0.05f,
    };
    g_DebugArrowVertexCount = 10;

    glGenVertexArrays(1, &g_DebugArrowVAO);
    glBindVertexArray(g_DebugArrowVAO);
    glGenBuffers(1, &g_DebugArrowVBO);
    glBindBuffer(GL_ARRAY_BUFFER, g_DebugArrowVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void DrawDebugArrow(const glm::vec4 &origin, const glm::vec4 &direction, const glm::vec4 &color)
{
    if (g_DebugArrowVAO == 0)
        return;

    float len = norm(direction);
    if (len < 1e-6f)
        return;

    glm::vec4 dir_n = direction / len;

    glm::vec4 x_axis(1.0f, 0.0f, 0.0f, 0.0f);
    float dot = dotproduct(x_axis, dir_n);
    dot = std::max(-1.0f, std::min(1.0f, dot));

    glm::mat4 model;
    if (std::abs(dot - 1.0f) < 1e-6f)
    {
        model = Matrix_Translate(origin.x, origin.y, origin.z) *
                Matrix_Scale(len, 1.0f, 1.0f);
    }
    else if (std::abs(dot + 1.0f) < 1e-6f)
    {
        model = Matrix_Translate(origin.x, origin.y, origin.z) *
                Matrix_Scale(-len, 1.0f, 1.0f);
    }
    else
    {
        float angle = std::acos(dot);
        glm::vec4 axis = crossproduct(x_axis, dir_n);
        axis = axis / norm(axis);
        model = Matrix_Translate(origin.x, origin.y, origin.z) *
                Matrix_Rotate(angle, axis) *
                Matrix_Scale(len, 1.0f, 1.0f);
    }

    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(g_object_id_uniform, OBJECT_ID_DEBUG_CUBE);

    GLint color_loc = glGetUniformLocation(g_GpuProgramID, "debug_color");
    if (color_loc >= 0)
        glUniform4f(color_loc, color.x, color.y, color.z, color.w);

    glDisable(GL_CULL_FACE);
    glLineWidth(2.0f);
    glBindVertexArray(g_DebugArrowVAO);
    glDrawArrays(GL_LINES, 0, g_DebugArrowVertexCount);
    glBindVertexArray(0);
    glLineWidth(1.0f);
    glEnable(GL_CULL_FACE);
}

static std::string ResolveScene00Path(const char *relative_from_bin, const char *relative_from_root)
{
    std::ifstream test_bin(relative_from_bin);
    if (test_bin.good())
        return std::string(relative_from_bin);

    std::ifstream test_root(relative_from_root);
    if (test_root.good())
        return std::string(relative_from_root);

    return std::string(relative_from_bin);
}

float WrapAnglePi(float angle)
{
    while (angle > 3.141592f)
        angle -= 2.0f * 3.141592f;
    while (angle < -3.141592f)
        angle += 2.0f * 3.141592f;
    return angle;
}

float SmoothFollowAngle(float current, float target, float speed, float dt)
{
    const float alpha = 1.0f - std::exp(-speed * dt);
    const float delta = WrapAnglePi(target - current);
    return WrapAnglePi(current + delta * alpha);
}

static glm::vec4 EvaluateBezierCurve(
    const glm::vec4 &p0,
    const glm::vec4 &p1,
    const glm::vec4 &p2,
    const glm::vec4 &p3,
    float t)
{
    const float one_minus_t = 1.0f - t;
    const float b0 = one_minus_t * one_minus_t * one_minus_t;
    const float b1 = 3.0f * one_minus_t * one_minus_t * t;
    const float b2 = 3.0f * one_minus_t * t * t;
    const float b3 = t * t * t;
    return b0 * p0 + b1 * p1 + b2 * p2 + b3 * p3;
}

static float Clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

static float SmoothApproach(float current, float target, float speed, float dt)
{
    const float alpha = 1.0f - std::exp(-speed * dt);
    return current + (target - current) * alpha;
}

static glm::vec4 ComputeFirstPersonCameraOffset()
{
    const float eye_height = std::max(0.4f, g_PlayerCubeHalfExtents.y * 0.82f);
    glm::vec4 local_eye_offset = g_PositionCameraFirstPerson;
    local_eye_offset.y = eye_height;
    return Matrix_Rotate_Y(-g_PlayerYaw) * local_eye_offset;
}

static float GetSlingshotPullAmount()
{
    return Clamp01(g_SlingshotState.charge_time_seconds / g_SlingshotState.max_charge_time_seconds);
}

static glm::vec4 ComputeProjectileSpawnPosition(const glm::vec4 &camera_position, const glm::vec4 &view_direction)
{
    return camera_position + 0.35f * view_direction;
}

static void BeginSlingshotCharge()
{
    if (g_SlingshotState.shot_cooldown_timer > 0.0f)
        return;

    g_SlingshotState.is_charging = true;
    g_SlingshotState.fire_requested = false;
    g_SlingshotState.charge_time_seconds = 0.0f;
    g_SlingshotState.queued_charge_ratio = 0.0f;
}

static void QueueSlingshotShot()
{
    if (g_SlingshotState.shot_cooldown_timer > 0.0f)
    {
        g_SlingshotState.is_charging = false;
        g_SlingshotState.charge_time_seconds = 0.0f;
        g_SlingshotState.queued_charge_ratio = 0.0f;
        return;
    }

    g_SlingshotState.is_charging = false;
    g_SlingshotState.fire_requested = true;
    g_SlingshotState.queued_charge_ratio = GetSlingshotPullAmount();
    g_SlingshotState.charge_time_seconds = 0.0f;
}

static ProjectileCollisionResult QueryProjectileCollision(const ProjectileState &projectile)
{
    const glm::vec4 projectile_half_extents(projectile.radius, projectile.radius, projectile.radius, 0.0f);
    const CollisionShapeType scenario_hit =
        CollidesWithScenario(projectile.position, g_ScenarioCollisionShapes, projectile_half_extents);

    if (scenario_hit == CollisionShapeType::SOLID || scenario_hit == CollisionShapeType::DOOR)
    {
        return {ProjectileCollisionTarget::SCENARIO, scenario_hit, -1, -1};
    }

    const int enemy_index = QueryEnemyHitByPlayerProjectile(projectile.position, projectile.radius);
    if (enemy_index >= 0)
        return {ProjectileCollisionTarget::ENEMY, CollisionShapeType::NONE, enemy_index, -1};

    // TODO: Reintegrar colisÃ£o do estilingue com objetos interativos, se necessÃ¡rio.
    return {ProjectileCollisionTarget::NONE, CollisionShapeType::NONE, -1, -1};
}

static void FireSlingshotProjectile(const glm::vec4 &camera_position, const glm::vec4 &view_direction)
{
    const float charge_ratio = g_SlingshotState.queued_charge_ratio;
    const float min_projectile_speed = 10.0f;
    const float max_projectile_speed = 24.0f;

    g_SlingshotProjectile.is_active = true;
    g_SlingshotProjectile.position = ComputeProjectileSpawnPosition(camera_position, view_direction);
    g_SlingshotProjectile.direction = view_direction;
    g_SlingshotProjectile.speed =
        min_projectile_speed + (max_projectile_speed - min_projectile_speed) * charge_ratio;
    g_SlingshotProjectile.lifetime_seconds = 0.0f;

    g_SlingshotState.fire_requested = false;
    g_SlingshotState.queued_charge_ratio = 0.0f;
    g_SlingshotState.shot_cooldown_timer = g_SlingshotState.shot_cooldown_seconds;
}

static void UpdateSlingshotProjectile(float delta_time)
{
    if (!g_SlingshotProjectile.is_active)
        return;

    g_SlingshotProjectile.position +=
        g_SlingshotProjectile.direction * (g_SlingshotProjectile.speed * delta_time);
    g_SlingshotProjectile.lifetime_seconds += delta_time;

    if (g_SlingshotProjectile.lifetime_seconds >= g_SlingshotProjectile.max_lifetime_seconds)
    {
        g_SlingshotProjectile.is_active = false;
        return;
    }

    const ProjectileCollisionResult hit_result = QueryProjectileCollision(g_SlingshotProjectile);
    if (hit_result.target != ProjectileCollisionTarget::NONE)
    {
        if (hit_result.target == ProjectileCollisionTarget::ENEMY && hit_result.enemy_index >= 0)
        {
            ApplyPlayerProjectileDamageToEnemy(hit_result.enemy_index, 1);
            // TODO: Integrar dano do estilingue com o futuro sistema de ataque do jogador.
        }

        g_SlingshotProjectile.is_active = false;
    }
}

#if 0 // Lógica de inimigos migrada para src/enemies.cpp
static void UpdateDekuScrub(Enemy &enemy, float delta_time)
{
    const glm::vec4 delta_to_player = g_PlayerCubePosition - enemy.position;
    const float distance_to_player = DistanceXZ(enemy.position, g_PlayerCubePosition);

    enemy.vulnerable = (enemy.state == EnemyState::Stunned);
    enemy.yaw = ComputeYawToTarget(enemy.position, g_PlayerCubePosition);

    if (enemy.attack_cooldown_timer > 0.0f)
        enemy.attack_cooldown_timer = std::max(0.0f, enemy.attack_cooldown_timer - delta_time);

    switch (enemy.state)
    {
    case EnemyState::Hidden:
        enemy.visible = true;
        enemy.position.y = enemy.spawn_position.y + DEKU_SCRUB_HIDDEN_HEIGHT;
        if (distance_to_player <= enemy.detection_radius)
        {
            enemy.state = EnemyState::Aiming;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Aiming:
        enemy.position.y = enemy.spawn_position.y;
        if (distance_to_player > enemy.detection_radius * 1.4f)
        {
            enemy.state = EnemyState::Hidden;
            enemy.state_timer = 0.0f;
            break;
        }

        if (enemy.attack_cooldown_timer <= 0.0f && enemy.state_timer >= 0.8f)
        {
            enemy.state = EnemyState::Shooting;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Shooting:
        enemy.position.y = enemy.spawn_position.y;
        if (!enemy.has_spawned_helpers && enemy.state_timer >= 0.15f)
        {
            SpawnDekuScrubProjectile(enemy, delta_to_player);
            enemy.has_spawned_helpers = true;
        }

        if (enemy.state_timer >= 0.45f)
        {
            enemy.has_spawned_helpers = false;
            enemy.attack_cooldown_timer = enemy.attack_cooldown;
            enemy.state = EnemyState::Aiming;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Stunned:
        enemy.position.y = enemy.spawn_position.y;
        enemy.vulnerable = true;
        if (enemy.state_timer >= DEKU_SCRUB_STUN_DURATION)
        {
            enemy.state = EnemyState::Hidden;
            enemy.state_timer = 0.0f;
        }
        break;

    default:
        enemy.state = EnemyState::Hidden;
        enemy.state_timer = 0.0f;
        break;
    }

    // TODO: Integrar reflexão do escudo para colocar o Deku Scrub em Stunned.
}

static void UpdateSkullwalltula(Enemy &enemy, float delta_time)
{
    (void)delta_time;
    enemy.position.x = enemy.spawn_position.x + 0.12f * std::sin(enemy.animation_timer * 0.7f);
    enemy.position.y = enemy.spawn_position.y;
    enemy.yaw = ComputeYawToTarget(enemy.position, g_PlayerCubePosition);
    enemy.vulnerable = true;

    if (DistanceXZ(enemy.position, g_PlayerCubePosition) <= enemy.attack_radius)
    {
        LogPlayerHitByEnemy("Skullwalltula");
        // TODO: Aplicar dano/knockback ao jogador ao aproximar de Skullwalltula.
    }
}

static void UpdateBigSkulltula(Enemy &enemy, float delta_time)
{
    const glm::vec4 to_player = NormalizeXZ(g_PlayerCubePosition - enemy.position);
    const glm::vec4 enemy_forward = ForwardFromYaw(enemy.yaw);
    const float front_dot = dotproduct(enemy_forward, to_player);

    enemy.yaw = WrapAnglePi(enemy.yaw + 0.55f * delta_time);
    enemy.vulnerable = false;

    switch (enemy.state)
    {
    case EnemyState::Idle:
    case EnemyState::Turning:
        enemy.state = EnemyState::Turning;
        if (front_dot < -0.25f)
        {
            enemy.state = EnemyState::Vulnerable;
            enemy.state_timer = 0.0f;
        }
        else if (DistanceXZ(enemy.position, g_PlayerCubePosition) <= enemy.attack_radius &&
                 enemy.attack_cooldown_timer <= 0.0f)
        {
            enemy.state = EnemyState::Attacking;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Vulnerable:
        enemy.vulnerable = true;
        if (front_dot >= -0.05f || enemy.state_timer >= BIG_SKULLTULA_VULNERABLE_DURATION)
        {
            enemy.state = EnemyState::Turning;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Attacking:
    {
        const glm::vec4 dash = ForwardFromYaw(enemy.yaw) * (1.8f * delta_time);
        MoveEnemyWithScenarioCollision(enemy, dash);

        if (enemy.state_timer >= BIG_SKULLTULA_ATTACK_DURATION)
        {
            enemy.attack_cooldown_timer = enemy.attack_cooldown;
            enemy.state = EnemyState::Vulnerable;
            enemy.state_timer = 0.0f;
        }

        if (DistanceXZ(enemy.position, g_PlayerCubePosition) <= enemy.attack_radius + 0.45f)
            LogPlayerHitByEnemy("Big Skulltula");
        // TODO: Aplicar dano no contato da investida/giro quando houver vida do jogador.
        break;
    }

    default:
        enemy.state = EnemyState::Turning;
        enemy.state_timer = 0.0f;
        break;
    }

    if (enemy.attack_cooldown_timer > 0.0f)
        enemy.attack_cooldown_timer = std::max(0.0f, enemy.attack_cooldown_timer - delta_time);
}

static void UpdateGohmaLarva(Enemy &enemy, float delta_time)
{
    const glm::vec4 delta_to_player = g_PlayerCubePosition - enemy.position;
    const float distance_to_player = DistanceXZ(enemy.position, g_PlayerCubePosition);
    enemy.vulnerable = true;

    switch (enemy.state)
    {
    case EnemyState::Idle:
        if (distance_to_player <= enemy.detection_radius)
        {
            enemy.state = EnemyState::Chasing;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::Chasing:
    {
        const glm::vec4 direction = NormalizeXZ(delta_to_player);
        enemy.yaw = ComputeYawToTarget(enemy.position, g_PlayerCubePosition);
        MoveEnemyWithScenarioCollision(enemy, direction * (1.9f * delta_time));

        if (distance_to_player <= enemy.attack_radius && enemy.attack_cooldown_timer <= 0.0f)
        {
            enemy.state = EnemyState::Jumping;
            enemy.state_timer = 0.0f;
            enemy.attack_cooldown_timer = enemy.attack_cooldown;
        }
        break;
    }

    case EnemyState::Jumping:
    {
        const glm::vec4 direction = NormalizeXZ(delta_to_player);
        enemy.yaw = ComputeYawToTarget(enemy.position, g_PlayerCubePosition);
        MoveEnemyWithScenarioCollision(enemy, direction * (3.6f * delta_time));

        const float jump_progress = Clamp01(enemy.state_timer / GOHMA_LARVA_JUMP_DURATION);
        enemy.position.y = enemy.spawn_position.y + std::sin(jump_progress * PI) * 0.45f;

        if (enemy.state_timer >= GOHMA_LARVA_JUMP_DURATION)
        {
            enemy.position.y = enemy.spawn_position.y;
            enemy.state = EnemyState::Chasing;
            enemy.state_timer = 0.0f;
        }

        if (DistanceXZ(enemy.position, g_PlayerCubePosition) <= enemy.attack_radius + 0.35f &&
            jump_progress >= 0.65f)
            LogPlayerHitByEnemy("Gohma Larva");
        // TODO: Aplicar dano/empurrão ao jogador na aterrissagem da larva.
        break;
    }

    default:
        enemy.state = EnemyState::Idle;
        enemy.state_timer = 0.0f;
        break;
    }

    if (enemy.attack_cooldown_timer > 0.0f)
        enemy.attack_cooldown_timer = std::max(0.0f, enemy.attack_cooldown_timer - delta_time);
}

static void UpdateQueenGohma(Enemy &enemy, float delta_time)
{
    const float distance_to_player = DistanceXZ(enemy.position, g_PlayerCubePosition);
    enemy.yaw = ComputeYawToTarget(enemy.position, g_PlayerCubePosition);
    enemy.vulnerable = false;

    switch (enemy.state)
    {
    case EnemyState::BossIdle:
        if (enemy.state_timer >= 1.1f)
        {
            enemy.state = EnemyState::BossLookAtPlayer;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::BossLookAtPlayer:
        enemy.vulnerable = true; // olho exposto enquanto observa o jogador.
        if (distance_to_player <= enemy.attack_radius && enemy.attack_cooldown_timer <= 0.0f)
        {
            enemy.state = EnemyState::BossAttack;
            enemy.state_timer = 0.0f;
        }
        else if (enemy.state_timer >= 3.8f)
        {
            enemy.state = EnemyState::BossClimb;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::BossAttack:
    {
        const glm::vec4 dash = ForwardFromYaw(enemy.yaw) * (2.1f * delta_time);
        MoveEnemyWithScenarioCollision(enemy, dash);
        enemy.vulnerable = (enemy.state_timer >= 0.55f);

        if (enemy.state_timer >= 1.2f)
        {
            enemy.attack_cooldown_timer = 2.5f;
            enemy.state = EnemyState::BossLookAtPlayer;
            enemy.state_timer = 0.0f;
        }

        if (DistanceXZ(enemy.position, g_PlayerCubePosition) <= enemy.attack_radius + 0.80f)
            LogPlayerHitByEnemy("Queen Gohma");
        // TODO: Aplicar dano da investida da Queen Gohma no jogador.
        break;
    }

    case EnemyState::BossClimb:
        enemy.position.y = enemy.spawn_position.y + std::min(2.8f, enemy.state_timer * 1.6f);
        if (enemy.state_timer >= 2.0f)
        {
            enemy.state = EnemyState::BossDropEggs;
            enemy.state_timer = 0.0f;
            enemy.has_spawned_helpers = false;
        }
        break;

    case EnemyState::BossDropEggs:
        enemy.position.y = enemy.spawn_position.y + 2.8f;
        if (!enemy.has_spawned_helpers && enemy.state_timer >= 0.4f)
        {
            SpawnQueenGohmaEggWave(enemy);
            enemy.has_spawned_helpers = true;
        }

        if (enemy.state_timer >= 1.6f)
        {
            enemy.state = EnemyState::BossAttack;
            enemy.state_timer = 0.0f;
            enemy.position.y = enemy.spawn_position.y;
            enemy.has_spawned_helpers = false;
        }
        break;

    case EnemyState::BossStunned:
        enemy.vulnerable = true;
        if (enemy.state_timer >= QUEEN_GOHMA_STUN_DURATION)
        {
            enemy.state = EnemyState::BossLookAtPlayer;
            enemy.state_timer = 0.0f;
        }
        break;

    case EnemyState::BossDead:
        enemy.active = false;
        enemy.visible = false;
        break;

    default:
        enemy.state = EnemyState::BossIdle;
        enemy.state_timer = 0.0f;
        break;
    }

    if (enemy.attack_cooldown_timer > 0.0f)
        enemy.attack_cooldown_timer = std::max(0.0f, enemy.attack_cooldown_timer - delta_time);
}

void InitializeEnemies()
{
    g_Enemies.clear();
    g_PendingEnemySpawns.clear();
    g_EnemyProjectiles.clear();
    const float test_ground_y = g_HardcodedTestSpawnPosition.y - 4.0f;

    Enemy deku_baba = MakeEnemy(
        EnemyType::DEKU_BABA,
        EnemyState::Idle,
        glm::vec4(4.3f, test_ground_y, 2.4f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f),
        glm::vec4(0.45f, 0.95f, 0.45f, 0.0f),
        2,
        3.8f,
        1.3f,
        1.8f,
        "Dekubaba");
    deku_baba.debug_color = glm::vec4(0.42f, 0.82f, 0.28f, 1.0f);
    g_Enemies.push_back(deku_baba);

    Enemy withered = MakeEnemy(
        EnemyType::WITHERED_DEKU_BABA,
        EnemyState::Idle,
        glm::vec4(0.9f, test_ground_y, 4.6f, 1.0f),
        glm::vec4(0.72f, 0.72f, 0.72f, 0.0f),
        glm::vec4(0.38f, 0.60f, 0.38f, 0.0f),
        1,
        0.0f,
        0.0f,
        0.0f,
        "Dekubaba");
    withered.yaw = 0.35f;
    withered.timer_b = withered.yaw;
    withered.debug_color = glm::vec4(0.70f, 0.62f, 0.38f, 1.0f);
    g_Enemies.push_back(withered);

    Enemy deku_scrub = MakeEnemy(
        EnemyType::DEKU_SCRUB,
        EnemyState::Hidden,
        glm::vec4(6.8f, test_ground_y, -0.3f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f),
        glm::vec4(0.95f, 0.95f, 0.95f, 0.0f),
        2,
        6.2f,
        5.5f,
        1.7f,
        "nodes_12__meshes[2]");
    deku_scrub.debug_color = glm::vec4(0.86f, 0.58f, 0.22f, 1.0f);
    g_Enemies.push_back(deku_scrub);

    Enemy skullwalltula = MakeEnemy(
        EnemyType::SKULLWALLTULA,
        EnemyState::Idle,
        glm::vec4(-1.9f, test_ground_y, 5.2f, 1.0f),
        glm::vec4(1.20f, 1.20f, 1.20f, 0.0f),
        glm::vec4(0.90f, 1.30f, 0.50f, 0.0f),
        2,
        0.0f,
        1.1f,
        0.0f,
        "skullwalltula_placeholder");
    skullwalltula.debug_color = glm::vec4(0.88f, 0.18f, 0.18f, 1.0f);
    g_Enemies.push_back(skullwalltula);

    Enemy big_skulltula = MakeEnemy(
        EnemyType::BIG_SKULLTULA,
        EnemyState::Turning,
        glm::vec4(8.6f, test_ground_y, 5.1f, 1.0f),
        glm::vec4(2.70f, 2.70f, 2.70f, 0.0f),
        glm::vec4(1.40f, 1.50f, 1.40f, 0.0f),
        3,
        0.0f,
        1.8f,
        2.4f,
        "big_skulltula_placeholder");
    big_skulltula.debug_color = glm::vec4(0.96f, 0.58f, 0.14f, 1.0f);
    g_Enemies.push_back(big_skulltula);

    Enemy larva = MakeEnemy(
        EnemyType::GOHMA_LARVA,
        EnemyState::Idle,
        glm::vec4(10.2f, test_ground_y, 2.0f, 1.0f),
        glm::vec4(1.44f, 1.44f, 1.44f, 0.0f),
        glm::vec4(0.64f, 0.40f, 0.64f, 0.0f),
        2,
        5.5f,
        1.0f,
        1.8f,
        "gohma_larva_placeholder");
    larva.vulnerable = true;
    larva.debug_color = glm::vec4(0.25f, 0.82f, 0.36f, 1.0f);
    g_Enemies.push_back(larva);

    Enemy queen_gohma = MakeEnemy(
        EnemyType::QUEEN_GOHMA,
        EnemyState::BossIdle,
        glm::vec4(13.8f, test_ground_y, -1.6f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f),
        glm::vec4(1.45f, 1.25f, 1.45f, 0.0f),
        8,
        12.0f,
        3.2f,
        2.5f,
        "Queen_Gohma");
    queen_gohma.yaw = -PI * 0.5f;
    queen_gohma.debug_color = glm::vec4(0.62f, 0.25f, 0.22f, 1.0f);
    g_Enemies.push_back(queen_gohma);
}

void UpdateEnemy(size_t enemy_index, float delta_time)
{
    Enemy &enemy = g_Enemies[enemy_index];
    if (!enemy.active || enemy.dead)
        return;

    enemy.state_timer += delta_time;
    enemy.animation_timer += delta_time;

    switch (enemy.type)
    {
    case EnemyType::DEKU_BABA:
        UpdateDekuBaba(enemy, delta_time);
        break;
    case EnemyType::WITHERED_DEKU_BABA:
        UpdateWitheredDekuBaba(enemy, delta_time);
        break;
    case EnemyType::DEKU_SCRUB:
        UpdateDekuScrub(enemy, delta_time);
        break;
    case EnemyType::SKULLWALLTULA:
        UpdateSkullwalltula(enemy, delta_time);
        break;
    case EnemyType::BIG_SKULLTULA:
        UpdateBigSkulltula(enemy, delta_time);
        break;
    case EnemyType::GOHMA_LARVA:
        UpdateGohmaLarva(enemy, delta_time);
        break;
    case EnemyType::QUEEN_GOHMA:
        UpdateQueenGohma(enemy, delta_time);
        break;
    }
}

void UpdateEnemies(float delta_time)
{
    if (g_PlayerHitLogCooldown > 0.0f)
        g_PlayerHitLogCooldown = std::max(0.0f, g_PlayerHitLogCooldown - delta_time);

    for (size_t i = 0; i < g_Enemies.size(); ++i)
        UpdateEnemy(i, delta_time);

    if (!g_PendingEnemySpawns.empty())
    {
        g_Enemies.insert(g_Enemies.end(), g_PendingEnemySpawns.begin(), g_PendingEnemySpawns.end());
        g_PendingEnemySpawns.clear();
    }
}

void UpdateEnemyProjectiles(float delta_time)
{
    for (size_t i = 0; i < g_EnemyProjectiles.size(); ++i)
    {
        EnemyProjectile &projectile = g_EnemyProjectiles[i];
        if (!projectile.active)
            continue;

        projectile.position += projectile.velocity * delta_time;
        projectile.lifetime_seconds += delta_time;

        const glm::vec4 projectile_half_extents = projectile.scale;
        const CollisionShapeType scenario_hit =
            CollidesWithScenario(projectile.position, g_ScenarioCollisionShapes, projectile_half_extents);

        if (projectile.lifetime_seconds >= projectile.max_lifetime_seconds ||
            scenario_hit == CollisionShapeType::SOLID ||
            scenario_hit == CollisionShapeType::DOOR)
        {
            projectile.active = false;
            continue;
        }

        if (BoxesIntersect(g_PlayerCubePosition, g_PlayerCubeHalfExtents, projectile.position, projectile_half_extents))
        {
            projectile.active = false;
            LogPlayerHitByEnemy("projétil inimigo");
            // TODO: Integrar dano no jogador e reflexão de escudo para projéteis inimigos.
        }
    }
}

void DrawEnemies()
{
    for (size_t i = 0; i < g_Enemies.size(); ++i)
    {
        const Enemy &enemy = g_Enemies[i];
        if (!enemy.active || enemy.dead || !enemy.visible)
            continue;

        const RenderModelInfo &render_info = GetEnemyRenderInfo(enemy);
        if (!render_info.available || render_info.object_names.empty())
            continue;

        const glm::mat4 model = BuildEnemyModelMatrix(enemy);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(
            g_object_id_uniform,
            IsEnemyUsingPlaceholder(enemy) ? OBJECT_ID_SPHERE : OBJECT_ID_ENEMY);
        glUniform1i(g_cube_colliding_uniform, 0);
        glUniform3f(g_object_tint_uniform, enemy.debug_color.x, enemy.debug_color.y, enemy.debug_color.z);

        for (size_t object_index = 0; object_index < render_info.object_names.size(); ++object_index)
            DrawVirtualObject(render_info.object_names[object_index].c_str());
    }
}

void DrawEnemyProjectiles()
{
    const RenderModelInfo &render_info =
        g_DekuScrubProjectileRenderInfo.available ? g_DekuScrubProjectileRenderInfo : g_SphereRenderInfo;

    if (!render_info.available || render_info.object_names.empty())
        return;

    for (size_t i = 0; i < g_EnemyProjectiles.size(); ++i)
    {
        const EnemyProjectile &projectile = g_EnemyProjectiles[i];
        if (!projectile.active)
            continue;

        glm::mat4 model =
            Matrix_Translate(projectile.position.x, projectile.position.y, projectile.position.z) *
            Matrix_Scale(
                projectile.scale.x * render_info.base_scale,
                projectile.scale.y * render_info.base_scale,
                projectile.scale.z * render_info.base_scale);

        if (render_info.available)
            model = model * Matrix_Translate(-render_info.center.x, -render_info.center.y, -render_info.center.z);

        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(
            g_object_id_uniform,
            g_DekuScrubProjectileRenderInfo.available ? OBJECT_ID_SCENARIO : OBJECT_ID_PROJECTILE);
        glUniform1i(g_cube_colliding_uniform, 0);
        glUniform3f(g_object_tint_uniform, 0.85f, 0.68f, 0.30f);

        for (size_t object_index = 0; object_index < render_info.object_names.size(); ++object_index)
            DrawVirtualObject(render_info.object_names[object_index].c_str());
    }
}

#endif

struct FairyMotionParams
{
    float orbit_radius;
    float orbit_period_seconds;
    float vertical_oscillations_per_circle;
    float vertical_oscillation_amplitude;
    float head_height_offset;
};

static const FairyMotionParams g_FairyMotionParams = {
    0.75f, // orbit_radius
    6.0f,  // orbit_period_seconds
    9.0f,  // vertical_oscillations_per_circle
    0.05f, // vertical_oscillation_amplitude
    1.0f  // head_height_offset
};

static glm::vec4 ComputeFairyOffset(float orbit_progress, const FairyMotionParams &params)
{
    const float pi = 3.141592f;
    const float half_pi = pi / 2.0f;
    const float radius = params.orbit_radius;
    const float tangent_scale = 0.55228475f * radius;
    const float angle_step = half_pi;

    const float wrapped_progress = orbit_progress - std::floor(orbit_progress);
    const float segment_progress = wrapped_progress * 4.0f;
    const int segment_index = static_cast<int>(segment_progress) % 4;
    const float local_t = segment_progress - static_cast<float>(segment_index);

    const float angle_start = segment_index * angle_step;
    const float angle_end = angle_start + angle_step;

    const glm::vec4 p0(
        radius * std::cos(angle_start),
        0.0f,
        radius * std::sin(angle_start),
        0.0f);
    const glm::vec4 p3(
        radius * std::cos(angle_end),
        0.0f,
        radius * std::sin(angle_end),
        0.0f);

    const glm::vec4 tangent_start(
        -std::sin(angle_start),
        0.0f,
        std::cos(angle_start),
        0.0f);
    const glm::vec4 tangent_end(
        -std::sin(angle_end),
        0.0f,
        std::cos(angle_end),
        0.0f);

    glm::vec4 p1 = p0;
    p1.x += tangent_scale * tangent_start.x;
    p1.z += tangent_scale * tangent_start.z;

    glm::vec4 p2 = p3;
    p2.x -= tangent_scale * tangent_end.x;
    p2.z -= tangent_scale * tangent_end.z;

    glm::vec4 fairy_offset = EvaluateBezierCurve(p0, p1, p2, p3, local_t);
    fairy_offset.y =
        params.vertical_oscillation_amplitude *
        std::sin(2.0f * pi * params.vertical_oscillations_per_circle * wrapped_progress);

    return fairy_offset;
}

void ComputeObjBounds(ObjModel *model, glm::vec4 &bbox_min, glm::vec4 &bbox_max)
{
    bbox_min = glm::vec4(+std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), 1.0f);
    bbox_max = glm::vec4(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);

    for (size_t i = 0; i < model->attrib.vertices.size() / 3; ++i)
    {
        const glm::vec4 p(
            model->attrib.vertices[3 * i + 0],
            model->attrib.vertices[3 * i + 1],
            model->attrib.vertices[3 * i + 2],
            1.0f);
        bbox_min.x = std::min(bbox_min.x, p.x);
        bbox_min.y = std::min(bbox_min.y, p.y);
        bbox_min.z = std::min(bbox_min.z, p.z);
        bbox_max.x = std::max(bbox_max.x, p.x);
        bbox_max.y = std::max(bbox_max.y, p.y);
        bbox_max.z = std::max(bbox_max.z, p.z);
    }
}

int main()
{
    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
    // sistema operacional, onde poderemos renderizar com OpenGL.
    int success = glfwInit();
    if (!success)
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos o callback para impressão de erros da GLFW no terminal
    glfwSetErrorCallback(ErrorCallback);

    // Pedimos para utilizar OpenGL versão 3.3 (ou superior)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Pedimos para utilizar o perfil "core", isto é, utilizaremos somente as
    // funções modernas de OpenGL.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criamos uma janela do sistema operacional, com 800 colunas e 600 linhas
    // de pixels, e com título "INF01047 ...".
    GLFWwindow *window;
    window = glfwCreateWindow(800, 600, "INF01047 - Seu Cartao - Seu Nome", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos a função de callback que será chamada sempre que o usuário
    // pressionar alguma tecla do teclado ...
    glfwSetKeyCallback(window, KeyCallback);
    // ... ou clicar os botões do mouse ...
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    // ... ou movimentar o cursor do mouse em cima da janela ...
    glfwSetCursorPosCallback(window, CursorPosCallback);
    // ... ou rolar a "rodinha" do mouse.
    glfwSetScrollCallback(window, ScrollCallback);

    // Indicamos que as chamadas OpenGL deverão renderizar nesta janela
    glfwMakeContextCurrent(window);

    // Carregamento de todas funções definidas por OpenGL 3.3, utilizando a
    // biblioteca GLAD.
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // Definimos a função de callback que será chamada sempre que a janela for
    // redimensionada, por consequência alterando o tamanho do "framebuffer"
    // (região de memória onde são armazenados os pixels da imagem).
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 800, 600); // Forçamos a chamada do callback acima, para definir g_ScreenRatio.

    // Imprimimos no terminal informações sobre a GPU do sistema
    const GLubyte *vendor = glGetString(GL_VENDOR);
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *glversion = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados
    // para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
    LoadShadersFromFiles();

    // Build debug wireframe cube and arrow for hitbox and vector visualization
    BuildDebugWireframeCube();
    BuildDebugArrowVAO();

    // Carregamos duas imagens para serem utilizadas como textura
    LoadTextureImage("../../data/red_brick_diff_1k.jpg");        // TextureImage0
    LoadTextureImage("../../data/rocky_terrain_02_diff_1k.jpg"); // TextureImage1

    // Textura 1x1 cinza padrao para objetos de cena sem textura (Kd = 0.8, 0.8, 0.8)
    {
        glGenTextures(1, &g_DefaultGrayTextureID);
        glBindTexture(GL_TEXTURE_2D, g_DefaultGrayTextureID);
        const unsigned char gray[4] = {204, 204, 204, 255}; // 0.8 * 255 = 204
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Carregamos todas as 12 partes do mapa (scene00 a scene11).
    g_SceneParts.clear();
    g_ScenarioModelMatrix = Matrix_Identity();
    g_ScenarioBoundsMin = glm::vec4(+std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), 1.0f);
    g_ScenarioBoundsMax = glm::vec4(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);

    for (int scene_idx = 0; scene_idx < 12; ++scene_idx)
    {
        char buf[256];
        char buf_bin[256];
        snprintf(buf, sizeof(buf), "assets/scenes/scene%02d/map.obj", scene_idx);
        snprintf(buf_bin, sizeof(buf_bin), "../../assets/scenes/scene%02d/map.obj", scene_idx);
        const std::string map_path = ResolveScene00Path(buf_bin, buf);
        snprintf(buf, sizeof(buf), "assets/scenes/scene%02d/collision.obj", scene_idx);
        snprintf(buf_bin, sizeof(buf_bin), "../../assets/scenes/scene%02d/collision.obj", scene_idx);
        const std::string col_path = ResolveScene00Path(buf_bin, buf);

        printf("\n=== Carregando cena scene%02d ===\n", scene_idx);

        ScenePart part;
        snprintf(buf, sizeof(buf), "scene%02d", scene_idx);
        part.name = buf;

        // --- Carrega mapa visual ---
        ObjModel map_model(map_path.c_str());
        ComputeNormals(&map_model);

        // Prefixa nomes das shapes para evitar colisoes no g_VirtualScene
        std::string prefix = part.name + "_";
        std::vector<std::string> original_names;
        original_names.reserve(map_model.shapes.size());
        for (auto &shape : map_model.shapes)
        {
            original_names.push_back(shape.name);
            shape.name = prefix + shape.name;
        }

        BuildTrianglesAndAddToVirtualScene(&map_model, g_DefaultGrayTextureID);

        // Coleta todos os sub-objetos gerados (cada face group vira um _grpN)
        for (auto &[name, obj] : g_VirtualScene)
        {
            if (name.compare(0, prefix.size(), prefix) == 0)
                part.render_object_names.push_back(name);
        }

        // Computa AABB da cena a partir dos vertices do map.obj
        {
            const float maxval = std::numeric_limits<float>::max();
            part.bbox_min = glm::vec4(+maxval, +maxval, +maxval, 1.0f);
            part.bbox_max = glm::vec4(-maxval, -maxval, -maxval, 1.0f);
            for (size_t v = 0; v < map_model.attrib.vertices.size(); v += 3)
            {
                const float vx = map_model.attrib.vertices[v + 0];
                const float vy = map_model.attrib.vertices[v + 1];
                const float vz = map_model.attrib.vertices[v + 2];
                part.bbox_min.x = std::min(part.bbox_min.x, vx);
                part.bbox_min.y = std::min(part.bbox_min.y, vy);
                part.bbox_min.z = std::min(part.bbox_min.z, vz);
                part.bbox_max.x = std::max(part.bbox_max.x, vx);
                part.bbox_max.y = std::max(part.bbox_max.y, vy);
                part.bbox_max.z = std::max(part.bbox_max.z, vz);
            }
        }

        printf("  AABB: [%.1f,%.1f,%.1f] a [%.1f,%.1f,%.1f]\n",
               part.bbox_min.x, part.bbox_min.y, part.bbox_min.z,
               part.bbox_max.x, part.bbox_max.y, part.bbox_max.z);

        // --- Carrega colisao ---
        {
            ObjModel col_model(col_path.c_str());
            ComputeNormals(&col_model);
            glm::vec4 col_bbox_min, col_bbox_max;
            BuildCollisionDataIntoVector(&col_model, Matrix_Identity(),
                part.collision_shapes, col_bbox_min, col_bbox_max);
        }

        printf("  Shapes de colisao: %zu, objetos visuais: %zu\n",
               part.collision_shapes.size(), part.render_object_names.size());

        // Atualiza bounds globais
        g_ScenarioBoundsMin.x = std::min(g_ScenarioBoundsMin.x, part.bbox_min.x);
        g_ScenarioBoundsMin.y = std::min(g_ScenarioBoundsMin.y, part.bbox_min.y);
        g_ScenarioBoundsMin.z = std::min(g_ScenarioBoundsMin.z, part.bbox_min.z);
        g_ScenarioBoundsMax.x = std::max(g_ScenarioBoundsMax.x, part.bbox_max.x);
        g_ScenarioBoundsMax.y = std::max(g_ScenarioBoundsMax.y, part.bbox_max.y);
        g_ScenarioBoundsMax.z = std::max(g_ScenarioBoundsMax.z, part.bbox_max.z);

        g_SceneParts.push_back(part);
    }

    // Computa adjacencia entre as cenas (AABB overlap com margem)
    printf("\n=== Computando adjacencia entre cenas ===\n");
    const float adjacency_margin = 1.5f;
    for (size_t i = 0; i < g_SceneParts.size(); ++i)
    {
        for (size_t j = 0; j < g_SceneParts.size(); ++j)
        {
            if (i == j) continue;
            const ScenePart &a = g_SceneParts[i];
            const ScenePart &b = g_SceneParts[j];

            const bool overlap =
                (a.bbox_min.x - adjacency_margin <= b.bbox_max.x + adjacency_margin &&
                 a.bbox_max.x + adjacency_margin >= b.bbox_min.x - adjacency_margin) &&
                (a.bbox_min.y - adjacency_margin <= b.bbox_max.y + adjacency_margin &&
                 a.bbox_max.y + adjacency_margin >= b.bbox_min.y - adjacency_margin) &&
                (a.bbox_min.z - adjacency_margin <= b.bbox_max.z + adjacency_margin &&
                 a.bbox_max.z + adjacency_margin >= b.bbox_min.z - adjacency_margin);

            if (overlap)
            {
                g_SceneParts[i].adjacent_indices.push_back(j);
            }
        }
        printf("  scene%02zu: %zu adjacentes\n", i, g_SceneParts[i].adjacent_indices.size());
    }

    // Configuracao inicial de cenas ativas (baseado no spawn point)
    {
        g_CurrentActiveSceneIndices.clear();
        for (size_t i = 0; i < g_SceneParts.size(); ++i)
        {
            const ScenePart &part = g_SceneParts[i];
            if (g_HardcodedTestSpawnPosition.x >= part.bbox_min.x &&
                g_HardcodedTestSpawnPosition.x <= part.bbox_max.x &&
                g_HardcodedTestSpawnPosition.y >= part.bbox_min.y &&
                g_HardcodedTestSpawnPosition.y <= part.bbox_max.y &&
                g_HardcodedTestSpawnPosition.z >= part.bbox_min.z &&
                g_HardcodedTestSpawnPosition.z <= part.bbox_max.z)
            {
                g_CurrentActiveSceneIndices.push_back(i);
                for (size_t adj : g_SceneParts[i].adjacent_indices)
                    g_CurrentActiveSceneIndices.push_back(adj);
                // Remove duplicatas
                std::sort(g_CurrentActiveSceneIndices.begin(), g_CurrentActiveSceneIndices.end());
                g_CurrentActiveSceneIndices.erase(
                    std::unique(g_CurrentActiveSceneIndices.begin(), g_CurrentActiveSceneIndices.end()),
                    g_CurrentActiveSceneIndices.end());
                break;
            }
        }

        // Se nao encontrou cena exata, usa scene00 + adjacentes
        if (g_CurrentActiveSceneIndices.empty())
        {
            g_CurrentActiveSceneIndices.push_back(0);
            for (size_t adj : g_SceneParts[0].adjacent_indices)
                g_CurrentActiveSceneIndices.push_back(adj);
            std::sort(g_CurrentActiveSceneIndices.begin(), g_CurrentActiveSceneIndices.end());
            g_CurrentActiveSceneIndices.erase(
                std::unique(g_CurrentActiveSceneIndices.begin(), g_CurrentActiveSceneIndices.end()),
                g_CurrentActiveSceneIndices.end());
        }

        // Constroi g_ScenarioCollisionShapes inicial
        g_ScenarioCollisionShapes.clear();
        for (size_t idx : g_CurrentActiveSceneIndices)
        {
            for (auto &cs : g_SceneParts[idx].collision_shapes)
                g_ScenarioCollisionShapes.push_back(cs);
        }

        printf("Cenas ativas iniciais: %zu (total collision shapes: %zu)\n",
               g_CurrentActiveSceneIndices.size(), g_ScenarioCollisionShapes.size());
    }

    // Tambem populamos g_ScenarioObjectNames para compatibilidade com codigo legado
    g_ScenarioObjectNames.clear();
    for (const auto &part : g_SceneParts)
        for (const auto &name : part.render_object_names)
            g_ScenarioObjectNames.push_back(name);

    const std::string player_model_path = ResolveScene00Path("../../Sword and Shield Pack/childlink_v2_clean.fbx", "Sword and Shield Pack/childlink_v2_clean.fbx");
    const std::string fairy_model_path = ResolveScene00Path("../../assets/navi/Navi.obj", "assets/navi/Navi.obj");
    const std::string deku_baba_model_path = ResolveScene00Path("../../assets/enemies/Deku Baba/Deku Baba/Dekubaba.obj", "assets/enemies/Deku Baba/Deku Baba/Dekubaba.obj");
    const std::string deku_scrub_model_path = ResolveScene00Path("../../assets/enemies/Deku Scrub/Deku Scrub (Forest Stage).obj", "assets/enemies/Deku Scrub/Deku Scrub (Forest Stage).obj");
    const std::string deku_scrub_plant_model_path = ResolveScene00Path("../../assets/enemies/Deku Scrub/choronuts_plant/choronuts_plant.obj", "assets/enemies/Deku Scrub/choronuts_plant/choronuts_plant.obj");
    const std::string deku_scrub_projectile_model_path = ResolveScene00Path("../../assets/enemies/Deku Scrub/dnk_ball_model/dnk_ball_model.obj", "assets/enemies/Deku Scrub/dnk_ball_model/dnk_ball_model.obj");
    const std::string spider_model_path = ResolveScene00Path("../../assets/enemies/Spider/Only_Spider_with_Animations_Export.obj", "assets/enemies/Spider/Only_Spider_with_Animations_Export.obj");
    const std::string queen_gohma_model_path = ResolveScene00Path("../../assets/enemies/Boss Queen Gohma/Queen Gohma/Queen Gohma.obj", "assets/enemies/Boss Queen Gohma/Queen Gohma/Queen Gohma.obj");



    // Carregamos o modelo visual do personagem principal usando Assimp (FBX).
    if (!g_PlayerModelLoader.LoadModel(player_model_path)) {
        fprintf(stderr, "ERROR: Failed to load player model from %s\n", player_model_path.c_str());
        std::exit(EXIT_FAILURE);
    }
    
    // Carrega animações separadamente
    std::string walkAnimPath = ResolveScene00Path("../../Sword and Shield Pack/sword and shield walk.fbx", "Sword and Shield Pack/sword and shield walk.fbx");
    printf("Walk animation path resolved: '%s'\n", walkAnimPath.c_str());
    if (!g_PlayerModelLoader.AddAnimation(walkAnimPath)) {
        printf("WARNING: Could not load walk animation from %s\n", walkAnimPath.c_str());
    }
    
    std::string idleAnimPath = ResolveScene00Path("../../Sword and Shield Pack/sword and shield idle.fbx", "Sword and Shield Pack/sword and shield idle.fbx");
    if (!g_PlayerModelLoader.AddAnimation(idleAnimPath)) {
        printf("WARNING: Could not load idle animation from %s\n", idleAnimPath.c_str());
    }
    
    std::string runAnimPath = ResolveScene00Path("../../Sword and Shield Pack/sword and shield run.fbx", "Sword and Shield Pack/sword and shield run.fbx");
    printf("Run animation path resolved: '%s'\n", runAnimPath.c_str());
    if (!g_PlayerModelLoader.AddAnimation(runAnimPath)) {
        printf("WARNING: Could not load run animation from %s\n", runAnimPath.c_str());
    }
    
    std::string attackAnimPath = ResolveScene00Path("../../Sword and Shield Pack/sword and shield attack.fbx", "Sword and Shield Pack/sword and shield attack.fbx");
    printf("Attack animation path resolved: '%s'\n", attackAnimPath.c_str());
    if (!g_PlayerModelLoader.AddAnimation(attackAnimPath)) {
        printf("WARNING: Could not load attack animation from %s\n", attackAnimPath.c_str());
    }
    
    std::string blockAnimPath = ResolveScene00Path("../../Sword and Shield Pack/sword and shield block idle.fbx", "Sword and Shield Pack/sword and shield block idle.fbx");
    printf("Block idle animation path resolved: '%s'\n", blockAnimPath.c_str());
    if (!g_PlayerModelLoader.AddAnimation(blockAnimPath)) {
        printf("WARNING: Could not load block idle animation from %s\n", blockAnimPath.c_str());
    }
    
    std::string jumpAnimPath = ResolveScene00Path("../../Sword and Shield Pack/sword and shield jump.fbx", "Sword and Shield Pack/sword and shield jump.fbx");
    printf("Jump animation path resolved: '%s'\n", jumpAnimPath.c_str());
    if (!g_PlayerModelLoader.AddAnimation(jumpAnimPath)) {
        printf("WARNING: Could not load jump animation from %s\n", jumpAnimPath.c_str());
    }
    
    printf("Total animations loaded: %zu\n", g_PlayerModelLoader.GetAnimations().size());
    
    BuildTrianglesFromAssimpAndAddToVirtualScene(g_PlayerModelLoader);
    
    // Calcula o AABB do modelo do personagem
    glm::vec4 player_model_bbox_min = glm::vec4(+std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), 1.0f);
    glm::vec4 player_model_bbox_max = glm::vec4(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);
    
    const auto &player_meshes = g_PlayerModelLoader.GetMeshes();
    for (const auto &mesh : player_meshes) {
        for (size_t i = 0; i < mesh.vertices.size() / 3; ++i) {
            const float vx = mesh.vertices[3 * i + 0];
            const float vy = mesh.vertices[3 * i + 1];
            const float vz = mesh.vertices[3 * i + 2];
            player_model_bbox_min.x = std::min(player_model_bbox_min.x, vx);
            player_model_bbox_min.y = std::min(player_model_bbox_min.y, vy);
            player_model_bbox_min.z = std::min(player_model_bbox_min.z, vz);
            player_model_bbox_max.x = std::max(player_model_bbox_max.x, vx);
            player_model_bbox_max.y = std::max(player_model_bbox_max.y, vy);
            player_model_bbox_max.z = std::max(player_model_bbox_max.z, vz);
        }
    }

    std::vector<std::string> player_model_object_names;
    player_model_object_names.reserve(player_meshes.size());
    for (size_t i = 0; i < player_meshes.size(); ++i)
    {
        player_model_object_names.push_back(player_meshes[i].name);
    }

    // Carrega texturas por mesh do jogador baseado no MTL
    // Mapeamento: mesh_index -> texture_file
    // baseado na correspondência de contagem de faces entre OBJ e FBX
    struct MeshTextureMapping {
        size_t meshIndex;
        const char* textureFile;
    };
    
    const std::vector<MeshTextureMapping> meshTextureMap = {
        {0,  "../../assets/char/childlink_02.png"},   // pants/gloves (47f)
        {1,  "../../assets/char/childlink_02.png"},   // pants/gloves (47f)
        {2,  "../../assets/char/c_mouth01.png"},      // mouth (46f)
        {3,  "../../assets/char/childlink_02.png"},   // pants/gloves (51f)
        {4,  "../../assets/char/childlink_01.png"},   // tunic (308f)
        {5,  "../../assets/char/childlink_00.png"},   // skin (549f)
        {6,  "../../assets/char/c_eye01.png"},        // eyes (79f)
        {7,  "../../assets/char/c_mouth01.png"},      // mouth (46f)
        {8,  "../../assets/char/childlink_01.png"},   // tunic (84f)
        {9,  "../../assets/char/childlink_f01.png"},  // hair2 (138f)
        {10, "../../assets/char/childlink_f00.png"},  // hair (242f)
        {11, "../../assets/char/c_eye01.png"},        // eyes (68f)
    };
    
    g_PlayerMeshTextureIDs.resize(player_meshes.size(), 0);
    
    // Cache de texturas para não carregar a mesma duas vezes
    std::map<std::string, GLuint> textureCache;
    
    stbi_set_flip_vertically_on_load(false);
    
    for (const auto& mapping : meshTextureMap) {
        if (mapping.meshIndex >= player_meshes.size()) continue;
        
        std::string texFile(mapping.textureFile);
        
        auto it = textureCache.find(texFile);
        if (it != textureCache.end()) {
            g_PlayerMeshTextureIDs[mapping.meshIndex] = it->second;
            continue;
        }
        
        int width, height, channels;
        unsigned char* data = stbi_load(texFile.c_str(), &width, &height, &channels, 4);
        if (data) {
            GLuint texID;
            glGenTextures(1, &texID);
            glBindTexture(GL_TEXTURE_2D, texID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            g_PlayerMeshTextureIDs[mapping.meshIndex] = texID;
            textureCache[texFile] = texID;
            
            printf("Player mesh %zu texture: '%s' (ID=%u, %dx%d)\n",
                   mapping.meshIndex, texFile.c_str(), texID, width, height);
            stbi_image_free(data);
        } else {
            printf("WARNING: Could not load texture '%s' for mesh %zu\n",
                   texFile.c_str(), mapping.meshIndex);
        }
    }
    
    stbi_set_flip_vertically_on_load(true);
    
    g_HasPlayerTexture = true;

    const glm::vec4 player_model_center = (player_model_bbox_min + player_model_bbox_max) * 0.5f;
    const glm::vec4 player_model_size = player_model_bbox_max - player_model_bbox_min;
    const float player_model_max_dimension = std::max(player_model_size.x, std::max(player_model_size.y, player_model_size.z));
    const float player_model_scale = (player_model_max_dimension > 1e-6f) ? (1.5f / player_model_max_dimension) : 1.0f;

    g_PlayerCubeHalfExtents = glm::vec4(
        player_model_size.x * player_model_scale * 0.5f,
        player_model_size.y * player_model_scale * 0.5f,
        player_model_size.z * player_model_scale * 0.5f,
        0.0f);

    // Mede stride length das animações e sincroniza velocidade de movimento
    printf("\n=== STRIDE LENGTH COMPUTATION ===\n");
    {
        const int walkAnimIdx = 1; // WALK animation index (matches GetAnimationIndex)
        const int runAnimIdx  = 3; // RUN animation index

        float walkSpeed = ComputeStrideLength(g_PlayerModelLoader, walkAnimIdx,
                                               player_model_scale, "WALK");
        float runSpeed  = ComputeStrideLength(g_PlayerModelLoader, runAnimIdx,
                                               player_model_scale, "RUN");

        if (walkSpeed > 0.0f) {
            g_PlayerStateMachine.SetWalkSpeed(walkSpeed);
            printf("[STRIDE] WALK speed updated: %.4f u/s (was 3.5)\n", walkSpeed);
        } else {
            printf("[STRIDE] WALK stride failed, keeping default %.4f u/s\n",
                   g_PlayerStateMachine.GetWalkSpeed());
        }

        if (runSpeed > 0.0f) {
            g_PlayerStateMachine.SetRunSpeed(runSpeed);
            printf("[STRIDE] RUN  speed updated: %.4f u/s (was 6.0)\n", runSpeed);
        } else {
            printf("[STRIDE] RUN  stride failed, keeping default %.4f u/s\n",
                   g_PlayerStateMachine.GetRunSpeed());
        }
    }
    printf("=== END STRIDE COMPUTATION ===\n\n");

    // Carregamos o modelo provisório da fada.
    ObjModel fairy_model(fairy_model_path.c_str());
    ComputeNormals(&fairy_model);
    BuildTrianglesAndAddToVirtualScene(&fairy_model);
    glm::vec4 fairy_model_bbox_min, fairy_model_bbox_max;
    ComputeObjBounds(&fairy_model, fairy_model_bbox_min, fairy_model_bbox_max);

    std::vector<std::string> fairy_model_object_names;
    // Coleta nomes gerados após split de face groups
    for (auto &[name, obj] : g_VirtualScene)
    {
        if (name.find("Navi_") == 0)
            fairy_model_object_names.push_back(name);
    }

    const glm::vec4 fairy_model_center = (fairy_model_bbox_min + fairy_model_bbox_max) * 0.5f;
    const glm::vec4 fairy_model_size = fairy_model_bbox_max - fairy_model_bbox_min;
    const float fairy_model_max_dimension = std::max(fairy_model_size.x, std::max(fairy_model_size.y, fairy_model_size.z));
    const float fairy_model_scale = (fairy_model_max_dimension > 1e-6f) ? (0.22f / fairy_model_max_dimension) : 1.0f;

    const std::string sphere_model_path = ResolveScene00Path("../../data/sphere.obj", "data/sphere.obj");
    ObjModel sphere_model(sphere_model_path.c_str());
    ComputeNormals(&sphere_model);
    BuildTrianglesAndAddToVirtualScene(&sphere_model);
    glm::vec4 sphere_model_bbox_min, sphere_model_bbox_max;
    ComputeObjBounds(&sphere_model, sphere_model_bbox_min, sphere_model_bbox_max);

    std::vector<std::string> sphere_model_object_names;
    // Coleta nomes gerados após split de face groups
    for (auto &[name, obj] : g_VirtualScene)
    {
        if (name.find("the_sphere") == 0)
            sphere_model_object_names.push_back(name);
    }

    const glm::vec4 sphere_model_center = (sphere_model_bbox_min + sphere_model_bbox_max) * 0.5f;

    // Dados de colisao ja foram construidos durante o loop de carregamento das cenas.

    ObjModel deku_baba_model(deku_baba_model_path.c_str());
    ComputeNormals(&deku_baba_model);
    BuildTrianglesAndAddToVirtualScene(&deku_baba_model);
    g_DekuBabaRenderInfo = BuildRenderModelInfo(deku_baba_model, 1.9f);

    ObjModel deku_scrub_model(deku_scrub_model_path.c_str());
    ComputeNormals(&deku_scrub_model);
    BuildTrianglesAndAddToVirtualScene(&deku_scrub_model);
    g_DekuScrubRenderInfo = BuildRenderModelInfo(deku_scrub_model, 1.5f);

    ObjModel deku_scrub_plant_model(deku_scrub_plant_model_path.c_str());
    ComputeNormals(&deku_scrub_plant_model);
    BuildTrianglesAndAddToVirtualScene(&deku_scrub_plant_model);
    g_DekuScrubPlantRenderInfo = BuildRenderModelInfo(deku_scrub_plant_model, 1.5f);

    ObjModel deku_scrub_projectile_model(deku_scrub_projectile_model_path.c_str());
    ComputeNormals(&deku_scrub_projectile_model);
    BuildTrianglesAndAddToVirtualScene(&deku_scrub_projectile_model);
    g_DekuScrubProjectileRenderInfo = BuildRenderModelInfo(deku_scrub_projectile_model, 0.35f);

    ObjModel spider_model(spider_model_path.c_str());
    ComputeNormals(&spider_model);
    BuildTrianglesAndAddToVirtualScene(&spider_model);
    g_SpiderRenderInfo = BuildRenderModelInfo(spider_model, 1.0f);

    ObjModel queen_gohma_model(queen_gohma_model_path.c_str());
    ComputeNormals(&queen_gohma_model);
    BuildTrianglesAndAddToVirtualScene(&queen_gohma_model);
    g_QueenGohmaRenderInfo = BuildRenderModelInfo(queen_gohma_model, 3.1f);

    // Spawn hardcoded para testes na scene00.
    g_PlayerCubePosition = g_HardcodedTestSpawnPosition;
    g_PlayerYaw = 0.0f;
    g_CameraYaw = g_PlayerYaw;
    g_CameraInitialized = false;
    InitializeEnemies(g_HardcodedTestSpawnPosition);

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();

    // Habilitamos o Z-buffer. Veja slides 104-116 do documento Aula_09_Projecoes.pdf.
    glEnable(GL_DEPTH_TEST);

    // Habilitamos o Blending para transparência
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Habilitamos o Backface Culling. Veja slides 8-13 do documento Aula_02_Fundamentos_Matematicos.pdf, slides 23-34 do documento Aula_13_Clipping_and_Culling.pdf e slides 112-123 do documento Aula_14_Laboratorio_3_Revisao.pdf.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    double previous_frame_time = glfwGetTime();

    // Ficamos em um loop infinito, renderizando, até que o usuário feche a ;janela
    while (!glfwWindowShouldClose(window))
    {
        const double current_frame_time = glfwGetTime();
        const float delta_time = static_cast<float>(current_frame_time - previous_frame_time);
        previous_frame_time = current_frame_time;

        // // Movimentação do personagem: W/S para frente/trás, A/D para girar.
        // Retorna direção do movimento para utilização na rotação da câmera.
        if (g_SlingshotState.is_charging)
        {
            g_SlingshotState.charge_time_seconds += delta_time;
            g_SlingshotState.charge_time_seconds =
                std::min(g_SlingshotState.charge_time_seconds, g_SlingshotState.max_charge_time_seconds);
        }

        // Detecta quais cenas do mapa estao ativas (player dentro + adjacentes)
        {
            size_t current_scene = g_SceneParts.size(); // invalido
            for (size_t i = 0; i < g_SceneParts.size(); ++i)
            {
                const ScenePart &part = g_SceneParts[i];
                if (g_PlayerCubePosition.x >= part.bbox_min.x &&
                    g_PlayerCubePosition.x <= part.bbox_max.x &&
                    g_PlayerCubePosition.y >= part.bbox_min.y &&
                    g_PlayerCubePosition.y <= part.bbox_max.y &&
                    g_PlayerCubePosition.z >= part.bbox_min.z &&
                    g_PlayerCubePosition.z <= part.bbox_max.z)
                {
                    current_scene = i;
                    break;
                }
            }

            if (current_scene >= g_SceneParts.size())
            {
                // Player fora de todas as cenas — fallback para scene00
                current_scene = 0;
            }

            std::vector<size_t> new_active;
            new_active.push_back(current_scene);
            for (size_t adj : g_SceneParts[current_scene].adjacent_indices)
                new_active.push_back(adj);
            std::sort(new_active.begin(), new_active.end());
            new_active.erase(std::unique(new_active.begin(), new_active.end()), new_active.end());

            if (new_active != g_CurrentActiveSceneIndices)
            {
                g_CurrentActiveSceneIndices = new_active;
                g_ScenarioCollisionShapes.clear();
                for (size_t idx : g_CurrentActiveSceneIndices)
                {
                    for (auto &cs : g_SceneParts[idx].collision_shapes)
                        g_ScenarioCollisionShapes.push_back(cs);
                }
            }
        }

        // State machine antes do movimento — precisa de onGround do frame anterior
        // para detectar transição de pulo (movement.cpp zera onGround internamente)
        g_PlayerStateMachine.Update(g_WPressed, g_SPressed, g_ShiftPressed,
                                    g_AttackPressed, g_DefendPressed, g_SpacePressed,
                                    g_IsClimbingAVine, g_IsClimbingALadder,
                                    g_PlayerOnGround, delta_time);

        // Consome input de ataque (single-shot)
        if (g_AttackPressed && g_PlayerStateMachine.GetCurrentState() == PlayerState::ATTACKING) {
            g_AttackPressed = false;
        }

        if (g_SlingshotState.shot_cooldown_timer > 0.0f)
        {
            g_SlingshotState.shot_cooldown_timer =
                std::max(0.0f, g_SlingshotState.shot_cooldown_timer - delta_time);
        }

        float move_input = UpdatePlayerMovement(window, delta_time);

        EnemyUpdateContext enemy_update_context;
        enemy_update_context.player_position = g_PlayerCubePosition;
        enemy_update_context.player_half_extents = g_PlayerCubeHalfExtents;
        enemy_update_context.scenario_collision_shapes = &g_ScenarioCollisionShapes;

        UpdateEnemies(delta_time, enemy_update_context);
        UpdateEnemyProjectiles(delta_time, enemy_update_context);

        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como branco.  Tal cor é
        // definida como coeficientes RGBA: Red, Green, Blue, Alpha; isto é:
        // Vermelho, Verde, Azul, Alpha (valor de transparência).
        // Conversaremos sobre sistemas de cores nas aulas de Modelos de Iluminação.
        //
        //           R     G     B     A
        glClearColor(0.9f, 0.9f, 1.0f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
        // e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
        // os shaders de vértice e fragmentos).
        glUseProgram(g_GpuProgramID);

        if (g_ThirdPersonCamera)
        {
            // Câmera third-person estilo Zelda-like:
            // gira suavemente para alinhar com o personagem e não orbita bruscamente.
            float camera_target_yaw = g_CameraYaw;

            if (std::fabs(move_input) > 1e-4f)
                camera_target_yaw = g_PlayerYaw;

            g_CameraYaw = SmoothFollowAngle(g_CameraYaw, camera_target_yaw, g_CameraYawFollowSpeed, delta_time);
            const glm::vec4 camera_back(std::sin(g_CameraYaw), 0.0f, -std::cos(g_CameraYaw), 0.0f);

            // Smooth Y do jogador para a câmera — evita trancos em step climbing
            static float g_CameraSmoothedPlayerY = g_PlayerCubePosition.y;
            g_CameraSmoothedPlayerY += (g_PlayerCubePosition.y - g_CameraSmoothedPlayerY)
                                       * std::min(1.0f, 8.0f * delta_time);
            glm::vec4 smoothedPlayerPos = g_PlayerCubePosition;
            smoothedPlayerPos.y = g_CameraSmoothedPlayerY;

            const glm::vec4 camera_lookat_world =
                smoothedPlayerPos + camera_back * (g_PlayerCubeHalfExtents.z + 0.18f) + glm::vec4(0.0f, g_ThirdPersonLookAtHeight, 0.0f, 0.0f);

            const glm::vec4 desired_camera_world =
                smoothedPlayerPos + camera_back * g_ThirdPersonCameraDistance + glm::vec4(0.0f, g_ThirdPersonCameraHeight, 0.0f, 0.0f);

            const glm::vec4 camera_path = desired_camera_world - camera_lookat_world;
            const float ideal_camera_distance = norm(camera_path);

            if (!g_CameraInitialized)
            {
                g_CurrentThirdPersonCameraDistance = ideal_camera_distance;
                g_CameraInitialized = true;
            }

            float target_camera_distance = ideal_camera_distance;
            float obstruction_distance = ideal_camera_distance;
            if (FindCameraObstructionDistance(camera_lookat_world, desired_camera_world, obstruction_distance))
            {
                target_camera_distance = std::max(CAMERA_MIN_DISTANCE, obstruction_distance - CAMERA_WALL_PADDING);
            }

            const float camera_follow_speed =
                (target_camera_distance < g_CurrentThirdPersonCameraDistance)
                    ? CAMERA_IN_SPEED
                    : CAMERA_OUT_SPEED;
            g_CurrentThirdPersonCameraDistance = SmoothApproach(
                g_CurrentThirdPersonCameraDistance,
                target_camera_distance,
                camera_follow_speed,
                delta_time);
            g_CurrentThirdPersonCameraDistance = std::max(CAMERA_MIN_DISTANCE, std::min(ideal_camera_distance, g_CurrentThirdPersonCameraDistance));

            const glm::vec4 camera_position_world =
                (ideal_camera_distance > 0.0001f)
                    ? camera_lookat_world + (camera_path / ideal_camera_distance) * g_CurrentThirdPersonCameraDistance
                    : desired_camera_world;
            // Abaixo definimos as varáveis que efetivamente definem a câmera virtual.
            camera_position_c = glm::vec4(camera_position_world.x, camera_position_world.y, camera_position_world.z, 1.0f); // Ponto "c", centro da câmera// Ponto "c", centro da câmera
            camera_lookat_l = glm::vec4(camera_lookat_world.x, camera_lookat_world.y, camera_lookat_world.z, 1.0f);         // Ponto "l", para onde a câmera (look-at) estará olhando
            camera_view_vector = camera_lookat_l - camera_position_c;                                                       // Vetor "view", sentido para onde a câmera está virada
            camera_up_vector = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);                                                           // Vetor "up" fixado para apontar para o "céu" (eito Y global)
        }

        else if (g_FirstPersonCamera)
        {
            //PrintVector(g_PositionCameraFirstPerson);
            glm::vec4 offset = ComputeFirstPersonCameraOffset();

            camera_position_c = g_PlayerCubePosition + offset;
            camera_view_vector = camera_view_vector;
            camera_up_vector = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        }

        if (glm::length(camera_view_vector) <= 0.001f)
        {
            camera_view_vector = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f); // Força um vetor válido apontando para frente
        }

        // Computamos a matriz "View" utilizando os parâmetros da câmera para
        // definir o sistema de coordenadas da câmera.  Veja slides 2-14, 184-190 e 236-242 do documento Aula_08_Sistemas_de_Coordenadas.pdf.
        const float camera_view_length = glm::length(camera_view_vector);
        const glm::vec4 projectile_view_direction =
            (camera_view_length > 0.001f)
                ? camera_view_vector / camera_view_length
                : glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

        if (g_SlingshotState.fire_requested && g_FirstPersonCamera)
        {
            FireSlingshotProjectile(camera_position_c, projectile_view_direction);
        }

        UpdateSlingshotProjectile(delta_time);

        glm::mat4 view = Matrix_Camera_View(camera_position_c, camera_view_vector, camera_up_vector);

        // Agora computamos a matriz de Projeção.
        glm::mat4 projection;

        // Note que, no sistema de coordenadas da câmera, os planos near e far
        // estão no sentido negativo! Veja slides 176-204 do documento Aula_09_Projecoes.pdf.
        float nearplane = -0.1f; // Posição do "near plane"
        float farplane = -80.0f; // Posição do "far plane"

        if (g_UsePerspectiveProjection)
        {
            // Projeção Perspectiva.
            // Para definição do field of view (FOV), veja slides 205-215 do documento Aula_09_Projecoes.pdf.
            float field_of_view = 3.141592 / 3.0f;
            projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
        }
        else
        {
            // Projeção Ortográfica.
            // Para definição dos valores l, r, b, t ("left", "right", "bottom", "top"),
            // PARA PROJEÇÃO ORTOGRÁFICA veja slides 219-224 do documento Aula_09_Projecoes.pdf.
            // Para simular um "zoom" ortográfico, computamos o valor de "t"
            // utilizando a variável g_CameraDistance.
            float t = 1.5f * g_CameraDistance / 2.5f;
            float b = -t;
            float r = t * g_ScreenRatio;
            float l = -r;
            projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
        }

        glm::mat4 model = Matrix_Identity(); // Transformação identidade de modelagem

        // Enviamos as matrizes "view" e "projection" para a placa de vídeo
        // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas são
        // efetivamente aplicadas em todos os pontos.
        glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(projection));

        // Desenhamos o cenário carregado da pasta assets/scenes.
        glDisable(GL_CULL_FACE);
        model = GetScenarioModelMatrix();
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(g_model_normal_matrix_uniform, 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(model))));
        glUniform1i(g_object_id_uniform, OBJECT_ID_SCENARIO);
        glUniform1i(g_cube_colliding_uniform, g_PlayerCubeColliding ? 1 : 0);
        glUniform3f(g_object_tint_uniform, 1.0f, 1.0f, 1.0f);
        for (size_t idx : g_CurrentActiveSceneIndices)
        {
            for (auto &name : g_SceneParts[idx].render_object_names)
            {
                DrawVirtualObject(name.c_str());
            }
        }
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // Desenhamos o modelo visual do personagem principal apenas em terceira pessoa.
        if (!g_FirstPersonCamera)
        {
            glDisable(GL_CULL_FACE);

            // visualPos = root - rootMotionCancel:
            //   Os ossos da animação contêm root motion (o Hips anda ~200u/ciclo em
            //   model-space).  Isso move o modelo visual para longe da hitbox no
            //   world-space e causa "teletransporte" no wrap do fmod.
            //   Para cancelar: subtraímos o deslocamento XZ do Hips de visualPos.
            //   O Y NÃO é subtraído — o bounce vertical já está correto nos ossos.
            glm::vec4 visualPos = g_PlayerCubePosition;

            if (g_PlayerModelLoader.GetNumBones() > 0) {
                glUniform1i(g_use_animation_uniform, 1);

                int animIdx = g_PlayerStateMachine.GetAnimationIndex();
                static int g_LastAnimIdx = -1;

                if (animIdx != g_LastAnimIdx) {
                    g_PlayerAnimationTime = 0.0f;
                    g_LastAnimIdx = animIdx;
                    printf("[ANIM DEBUG] Animation changed to index %d, animation time reset to 0\n", animIdx);
                }

                g_PlayerModelLoader.SetCurrentAnimation(animIdx);

                // Defesa (5) e pulo (6): play once, congela no último frame
                float tps = g_PlayerModelLoader.GetCurrentTicksPerSecond();
                float dur = g_PlayerModelLoader.GetCurrentDuration();
                float durSec = (tps > 0.0f) ? (dur / tps) : 1.0f;
                bool freezeAtEnd = (animIdx == 5 || animIdx == 6);

                if (freezeAtEnd && g_PlayerAnimationTime >= durSec - 0.01f) {
                    // Já chegou ao fim, mantém no último frame
                } else {
                    g_PlayerAnimationTime += delta_time * g_AnimationSpeedMultiplier;
                }

                g_AnimDebugFrameCounter++;
                if (g_AnimDebugFrameCounter % g_AnimDebugPrintInterval == 0) {
                    float timeInTicks = g_PlayerAnimationTime * tps;
                    float animCycleTime = fmod(timeInTicks, dur > 0.0f ? dur : 1.0f);
                    printf("[ANIM DEBUG] time=%.4fs tps=%.2f duration=%.2f ticks=%.2f cycle=%.2f speedMult=%.2f\n",
                           g_PlayerAnimationTime, tps, dur, timeInTicks, animCycleTime, g_AnimationSpeedMultiplier);
                }

                g_PlayerAnimationTransforms.resize(g_PlayerModelLoader.GetNumBones());
                g_PlayerModelLoader.GetBoneTransforms(g_PlayerAnimationTime, g_PlayerAnimationTransforms);

                for (size_t i = 0; i < g_PlayerAnimationTransforms.size(); i++) {
                    glUniformMatrix4fv(g_bones_uniform[i], 1, GL_FALSE, glm::value_ptr(g_PlayerAnimationTransforms[i]));
                }

                // Cancela root motion dos ossos subtraindo HipsDelta.xz de visualPos
                // Wrap detection apenas para animações com root motion (WALK=1, RUN=3)
                const glm::vec4& hipsPos = g_PlayerModelLoader.GetHipsWorldPositionRef();

                static int       hipsLastAnimIdx = -1;
                static glm::vec4 hipsRefPos(0.0f, 0.0f, 0.0f, 1.0f);
                static bool      hipsRefSet = false;
                static float     prevAnimTick = -1.0f;
                glm::vec4        hipsDelta(0.0f);

                bool hasRootMotion = (animIdx == 1 || animIdx == 3); // WALK ou RUN

                float animTick = fmod(g_PlayerAnimationTime * tps, dur > 0.0f ? dur : 1.0f);
                bool wrapped = hasRootMotion && prevAnimTick >= 0.0f && animTick < prevAnimTick;
                prevAnimTick = animTick;

                if (animIdx != hipsLastAnimIdx || wrapped || !hipsRefSet) {
                    hipsLastAnimIdx = animIdx;
                    hipsRefPos = hipsPos;
                    hipsDelta  = glm::vec4(0.0f);
                    hipsRefSet = true;
                    if (wrapped) {
                        printf("[HIPS WRAP] Cycle wrapped at animTick=%.2f, "
                               "hipsRef reset to (%.2f,%.2f,%.2f)\n",
                               animTick, hipsPos.x, hipsPos.y, hipsPos.z);
                    }
                } else {
                    hipsDelta = hipsPos - hipsRefPos;
                }

                if (g_AnimDebugFrameCounter % g_AnimDebugPrintInterval == 0) {
                    printf("[HIPS DEBUG] hipsPos=(%.1f,%.1f,%.1f) ref=(%.1f,%.1f,%.1f) "
                           "delta=(%.1f,%.1f,%.1f)\n",
                           hipsPos.x, hipsPos.y, hipsPos.z,
                           hipsRefPos.x, hipsRefPos.y, hipsRefPos.z,
                           hipsDelta.x, hipsDelta.y, hipsDelta.z);
                }

                // Cancela root motion: subtrai XZ, ignora Y (bounce já está nos ossos)
                glm::vec4 cancelDelta = hipsDelta;
                cancelDelta.y = 0.0f;

                glm::vec4 worldCancel = Matrix_Rotate_Y(-g_PlayerYaw)
                    * Matrix_Scale(player_model_scale, player_model_scale, player_model_scale)
                    * cancelDelta;

                visualPos = g_PlayerCubePosition - worldCancel;

                {
                    static int cancelLog = 0;
                    cancelLog++;
                    if (cancelLog % 10 == 0) {
                        glm::vec4 diff = visualPos - g_PlayerCubePosition;
                        printf("[ROOT CANCEL] delta=(%.1f,%.1f,%.1f) cancel=(%.3f,%.3f,%.3f) "
                               "visualPos=(%.3f,%.3f,%.3f) root=(%.3f,%.3f,%.3f) diff=(%.3f,%.3f,%.3f)\n",
                               hipsDelta.x, hipsDelta.y, hipsDelta.z,
                               worldCancel.x, worldCancel.y, worldCancel.z,
                               visualPos.x, visualPos.y, visualPos.z,
                               g_PlayerCubePosition.x, g_PlayerCubePosition.y, g_PlayerCubePosition.z,
                               diff.x, diff.y, diff.z);
                    }
                }
            } else {
                glUniform1i(g_use_animation_uniform, 0);
            }

            model = Matrix_Translate(visualPos.x, visualPos.y, visualPos.z)
                * Matrix_Rotate_Y(-g_PlayerYaw)
                * Matrix_Scale(player_model_scale, player_model_scale, player_model_scale)
                * Matrix_Translate(-player_model_center.x, -player_model_center.y, -player_model_center.z);

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(g_model_normal_matrix_uniform, 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(model))));
            glUniform1i(g_object_id_uniform, OBJECT_ID_PLAYER);
            glUniform1i(g_cube_colliding_uniform, g_PlayerCubeColliding ? 1 : 0);
            glUniform3f(g_object_tint_uniform, 1.0f, 1.0f, 1.0f);

            // Configura material do jogador
            if (!g_PlayerModelLoader.GetMaterials().empty()) {
                const auto& mat = g_PlayerModelLoader.GetMaterials()[0];
                glUniform3f(g_material_diffuse_uniform, mat.diffuse.x, mat.diffuse.y, mat.diffuse.z);
                glUniform3f(g_material_specular_uniform, mat.specular.x, mat.specular.y, mat.specular.z);
                glUniform3f(g_material_ambient_uniform, mat.ambient.x, mat.ambient.y, mat.ambient.z);
                glUniform1f(g_material_shininess_uniform, mat.shininess);
            }

            // Habilita textura do jogador
            glUniform1i(g_has_player_texture_uniform, g_HasPlayerTexture ? 1 : 0);
            glUniform1i(g_player_texture_uniform, 3);

            // Desenha cada mesh do jogador com sua textura
            for (size_t i = 0; i < player_model_object_names.size(); ++i)
            {
                // Liga a textura específica desta mesh na unit 3
                if (i < g_PlayerMeshTextureIDs.size() && g_PlayerMeshTextureIDs[i] != 0) {
                    glActiveTexture(GL_TEXTURE3);
                    glBindTexture(GL_TEXTURE_2D, g_PlayerMeshTextureIDs[i]);
                }
                DrawVirtualObject(player_model_object_names[i].c_str());
            }
        }
        
        // Reseta animação
        glUniform1i(g_use_animation_uniform, 0);

        EnemyDrawContext enemy_draw_context;
        enemy_draw_context.model_uniform = g_model_uniform;
        enemy_draw_context.object_id_uniform = g_object_id_uniform;
        enemy_draw_context.cube_colliding_uniform = g_cube_colliding_uniform;
        enemy_draw_context.object_tint_uniform = g_object_tint_uniform;
        enemy_draw_context.object_id_scenario = OBJECT_ID_SCENARIO;
        enemy_draw_context.object_id_sphere = OBJECT_ID_SPHERE;
        enemy_draw_context.object_id_projectile = OBJECT_ID_PROJECTILE;
        enemy_draw_context.object_id_enemy = OBJECT_ID_ENEMY;
        enemy_draw_context.render_resources.deku_baba_render_info = &g_DekuBabaRenderInfo;
        enemy_draw_context.render_resources.deku_scrub_render_info = &g_DekuScrubRenderInfo;
        enemy_draw_context.render_resources.deku_scrub_plant_render_info = &g_DekuScrubPlantRenderInfo;
        enemy_draw_context.render_resources.deku_scrub_projectile_render_info = &g_DekuScrubProjectileRenderInfo;
        enemy_draw_context.render_resources.spider_render_info = &g_SpiderRenderInfo;
        enemy_draw_context.render_resources.queen_gohma_render_info = &g_QueenGohmaRenderInfo;
        enemy_draw_context.render_resources.sphere_render_info = &g_SphereRenderInfo;
        enemy_draw_context.draw_virtual_object = DrawVirtualObject;

        DrawEnemies(enemy_draw_context);

        const float fairy_orbit_progress = std::fmod(static_cast<float>(current_frame_time) / g_FairyMotionParams.orbit_period_seconds, 1.0f);
        const glm::vec4 fairy_head_center = g_PlayerCubePosition + glm::vec4(0.0f, g_FairyMotionParams.head_height_offset, 0.0f, 0.0f);
        const glm::vec4 fairy_offset = ComputeFairyOffset(fairy_orbit_progress, g_FairyMotionParams);
        const glm::vec4 fairy_world_position = fairy_head_center + fairy_offset;

        const float pi = 3.141592f;
        const float vx = -fairy_offset.z;
        const float vz = fairy_offset.x;
        const float yaw = std::atan2(vx, vz) + pi / 2.0f;

        model =
            Matrix_Translate(fairy_world_position.x, fairy_world_position.y, fairy_world_position.z) *
            Matrix_Rotate_Y(yaw) *
            Matrix_Scale(fairy_model_scale, fairy_model_scale, fairy_model_scale) *
            Matrix_Translate(-fairy_model_center.x, -fairy_model_center.y, -fairy_model_center.z);

        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, OBJECT_ID_SCENARIO);
        glUniform1i(g_cube_colliding_uniform, 0);
        glUniform3f(g_object_tint_uniform, 0.55f, 0.95f, 0.70f);
        for (size_t i = 0; i < fairy_model_object_names.size(); ++i)
        {
            DrawVirtualObject(fairy_model_object_names[i].c_str());
        }

        if (g_SlingshotProjectile.is_active)
        {
            model =
                Matrix_Translate(g_SlingshotProjectile.position.x, g_SlingshotProjectile.position.y, g_SlingshotProjectile.position.z) *
                Matrix_Scale(g_SlingshotProjectile.radius, g_SlingshotProjectile.radius, g_SlingshotProjectile.radius) *
                Matrix_Translate(-sphere_model_center.x, -sphere_model_center.y, -sphere_model_center.z);

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, OBJECT_ID_PROJECTILE);
            glUniform1i(g_cube_colliding_uniform, 0);
            for (size_t i = 0; i < sphere_model_object_names.size(); ++i)
            {
                DrawVirtualObject(sphere_model_object_names[i].c_str());
            }
        }

        DrawEnemyProjectiles(enemy_draw_context);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // Debug drawing: render hitboxes if enabled
        if (g_ShowDebugHitboxes)
        {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);

            // Hitbox (verde) — colisão e visual são a mesma posição agora
            {
                glm::vec4 player_color(0.0f, 1.0f, 0.0f, 1.0f);
                CollisionOBB player_obb = {g_PlayerCubePosition, g_PlayerCubeHalfExtents, g_PlayerYaw};
                DrawDebugOBB(player_obb, player_color);
            }

            if (g_AnimDebugFrameCounter % g_AnimDebugPrintInterval == 0) {
                printf("[HITBOX DEBUG] pos=(%.4f,%.4f,%.4f) halfExt=(%.4f,%.4f,%.4f) yaw=%.4f\n",
                       g_PlayerCubePosition.x, g_PlayerCubePosition.y, g_PlayerCubePosition.z,
                       g_PlayerCubeHalfExtents.x, g_PlayerCubeHalfExtents.y, g_PlayerCubeHalfExtents.z,
                       g_PlayerYaw);
            }

            // Draw ladder hitboxes (yellow wireframe)
            for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
            {
                if (g_ScenarioCollisionShapes[i].type == CollisionShapeType::LADDER)
                {
                    glm::vec4 ladder_center = (g_ScenarioCollisionShapes[i].bbox_min + g_ScenarioCollisionShapes[i].bbox_max) * 0.5f;
                    glm::vec4 ladder_half = (g_ScenarioCollisionShapes[i].bbox_max - g_ScenarioCollisionShapes[i].bbox_min) * 0.5f;
                    ladder_center.w = 1.0f;
                    ladder_half.w = 0.0f;
                    glm::vec4 ladder_color(1.0f, 1.0f, 0.0f, 1.0f);
                    DrawDebugAABB(ladder_center, ladder_half, ladder_color);
                }
            }

            // Draw vine hitboxes (magenta wireframe)
            for (size_t i = 0; i < g_ScenarioCollisionShapes.size(); ++i)
            {
                if (g_ScenarioCollisionShapes[i].type == CollisionShapeType::VINES)
                {
                    glm::vec4 vine_center = (g_ScenarioCollisionShapes[i].bbox_min + g_ScenarioCollisionShapes[i].bbox_max) * 0.5f;
                    glm::vec4 vine_half = (g_ScenarioCollisionShapes[i].bbox_max - g_ScenarioCollisionShapes[i].bbox_min) * 0.5f;
                    vine_center.w = 1.0f;
                    vine_half.w = 0.0f;
                    glm::vec4 vine_color(1.0f, 0.0f, 1.0f, 1.0f);
                    DrawDebugAABB(vine_center, vine_half, vine_color);
                }
            }

            // Navi vectors debug display (3D arrows)
            {
                const float debug_navi_progress = std::fmod(static_cast<float>(current_frame_time) / g_FairyMotionParams.orbit_period_seconds, 1.0f);
                const glm::vec4 debug_navi_offset = ComputeFairyOffset(debug_navi_progress, g_FairyMotionParams);
                const glm::vec4 debug_navi_pos = g_PlayerCubePosition + glm::vec4(0.0f, g_FairyMotionParams.head_height_offset, 0.0f, 0.0f) + debug_navi_offset;

                const float debug_navi_w = debug_navi_progress - std::floor(debug_navi_progress);
                const float debug_navi_vx = -debug_navi_offset.z;
                const float debug_navi_vz = debug_navi_offset.x;
                const float debug_navi_vy = g_FairyMotionParams.vertical_oscillation_amplitude *
                                            2.0f * 3.141592f * g_FairyMotionParams.vertical_oscillations_per_circle *
                                            std::cos(2.0f * 3.141592f * g_FairyMotionParams.vertical_oscillations_per_circle * debug_navi_w);
                const float debug_navi_yaw = std::atan2(debug_navi_vx, debug_navi_vz) + 3.141592f / 2.0f;

                const float debug_arrow_len = 0.4f;
                glm::vec4 vel_dir(debug_navi_vx, debug_navi_vy, debug_navi_vz, 0.0f);
                float vel_len = norm(vel_dir);
                if (vel_len > 1e-6f)
                    vel_dir = vel_dir / vel_len;
                glm::vec4 forward_dir(-std::sin(debug_navi_yaw), 0.0f, -std::cos(debug_navi_yaw), 0.0f);

                DrawDebugArrow(debug_navi_pos, vel_dir * debug_arrow_len, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                DrawDebugArrow(debug_navi_pos, forward_dir * debug_arrow_len, glm::vec4(0.0f, 0.5f, 1.0f, 1.0f));
                DrawDebugArrow(debug_navi_pos, glm::vec4(0.0f, debug_arrow_len, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            }

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
        }

        // g_PlayerCubePosition nunca foi modificado — não precisa restaurar

        // Imprimimos na tela os ângulos de Euler que controlam a rotação do
        // terceiro cubo.
        TextRendering_ShowEulerAngles(window);

        // Imprimimos na informação sobre a matriz de projeção sendo utilizada.
        TextRendering_ShowProjection(window);

        if (g_ShowPlayerCoords)
        {
            char buf[128];
            float lineheight = TextRendering_LineHeight(window);
            snprintf(buf, sizeof(buf), "Pos: %.2f %.2f %.2f  Yaw: %.1f",
                     g_PlayerCubePosition.x, g_PlayerCubePosition.y, g_PlayerCubePosition.z,
                     g_PlayerYaw * 180.0f / 3.141592f);
            TextRendering_PrintString(window, buf, -1.0f, -1.0f + lineheight, 1.0f);
        }

        // Imprimimos na tela informação sobre o número de quadros renderizados
        // por segundo (frames per second).
        TextRendering_ShowFramesPerSecond(window);

        // O framebuffer onde OpenGL executa as operações de renderização não
        // é o mesmo que está sendo mostrado para o usuário, caso contrário
        // seria possível ver artefatos conhecidos como "screen tearing". A
        // chamada abaixo faz a troca dos buffers, mostrando para o usuário
        // tudo que foi renderizado pelas funções acima.
        // Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Frame rate cap: 60 FPS
        const double frame_end_time = glfwGetTime();
        const double frame_duration = frame_end_time - current_frame_time;
        const double min_frame_time = 1.0 / 60.0;
        if (frame_duration < min_frame_time) {
            glfwWaitEventsTimeout(min_frame_time - frame_duration);
        }
    }

    // Finalizamos o uso dos recursos do sistema operacional
    glfwTerminate();

    // Fim do programa
    return 0;
}

// Função que carrega uma imagem para ser utilizada como textura
GLuint LoadTextureImage(const char *filename)
{
    printf("Carregando imagem \"%s\"... ", filename);

    if (g_TextureCache.find(filename) != g_TextureCache.end())
    {
        return g_TextureCache[filename];
    }

    // Primeiro fazemos a leitura da imagem do disco
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 4);

    if (data == NULL)
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }

    printf("OK (%dx%d).\n", width, height);

    // Agora criamos objetos na GPU com OpenGL para armazenar a textura
    GLuint texture_id;
    GLuint sampler_id;
    glGenTextures(1, &texture_id);
    glGenSamplers(1, &sampler_id);

    // Veja slides 95-96 do documento Aula_20_Mapeamento_de_Texturas.pdf
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Parâmetros de amostragem da textura.
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    g_TextureCache[filename] = texture_id;
    g_SamplerCache[filename] = sampler_id;
    return texture_id;
}

// Função que desenha um objeto armazenado em g_VirtualScene. Veja definição
// dos objetos na função BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char *object_name)
{
    if (g_VirtualScene.find(object_name) == g_VirtualScene.end()) {
        printf("WARNING: Object '%s' not found in g_VirtualScene!\n", object_name);
        return;
    }
    
    // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
    // vértices apontados pelo VAO criado pela função BuildTrianglesAndAddToVirtualScene(). Veja
    // comentários detalhados dentro da definição de BuildTrianglesAndAddToVirtualScene().
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_VirtualScene[object_name].texture_id);
    glBindSampler(0, g_VirtualScene[object_name].sampler_id);

    GLint texture_unit_location = glGetUniformLocation(g_GpuProgramID, "TextureImage0");

    glUniform1i(texture_unit_location, 0);

    glm::vec4 bbox_min = g_VirtualScene[object_name].bbox_min;
    glm::vec4 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

    glDrawElements(
        g_VirtualScene[object_name].rendering_mode,
        g_VirtualScene[object_name].num_indices,
        GL_UNSIGNED_INT,
        (void *)(g_VirtualScene[object_name].first_index * sizeof(GLuint)));

    glBindVertexArray(0);
}

// Função que carrega os shaders de vértices e de fragmentos que serão
// utilizados para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
//
void LoadShadersFromFiles()
{
    // Note que o caminho para os arquivos "shader_vertex.glsl" e
    // "shader_fragment.glsl" estão fixados, sendo que assumimos a existência
    // da seguinte estrutura no sistema de arquivos:
    //
    //    + FCG_Lab_01/
    //    |
    //    +--+ bin/
    //    |  |
    //    |  +--+ Release/  (ou Debug/ ou Linux/)
    //    |     |
    //    |     o-- main.exe
    //    |
    //    +--+ src/
    //       |
    //       o-- shader_vertex.glsl
    //       |
    //       o-- shader_fragment.glsl
    //
    GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");

    // Deletamos o programa de GPU anterior, caso ele exista.
    if (g_GpuProgramID != 0)
        glDeleteProgram(g_GpuProgramID);

    // Criamos um programa de GPU utilizando os shaders carregados acima.
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
    g_model_uniform = glGetUniformLocation(g_GpuProgramID, "model");           // Variável da matriz "model"
    g_model_normal_matrix_uniform = glGetUniformLocation(g_GpuProgramID, "model_normal_matrix"); // Matriz de normais precomputada
    g_view_uniform = glGetUniformLocation(g_GpuProgramID, "view");             // Variável da matriz "view" em shader_vertex.glsl
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    g_object_id_uniform = glGetUniformLocation(g_GpuProgramID, "object_id");   // Variável "object_id" em shader_fragment.glsl
    g_bbox_min_uniform = glGetUniformLocation(g_GpuProgramID, "bbox_min");
    g_bbox_max_uniform = glGetUniformLocation(g_GpuProgramID, "bbox_max");
    g_cube_colliding_uniform = glGetUniformLocation(g_GpuProgramID, "cube_colliding");
    g_object_tint_uniform = glGetUniformLocation(g_GpuProgramID, "object_tint");

    // Uniforms para animação
    g_use_animation_uniform = glGetUniformLocation(g_GpuProgramID, "useAnimation");
    for (int i = 0; i < 100; i++) {
        std::string boneName = "bones[" + std::to_string(i) + "]";
        g_bones_uniform[i] = glGetUniformLocation(g_GpuProgramID, boneName.c_str());
    }

    // Uniforms para texturas e materiais do jogador
    g_player_texture_uniform = glGetUniformLocation(g_GpuProgramID, "playerTexture");
    g_has_player_texture_uniform = glGetUniformLocation(g_GpuProgramID, "hasPlayerTexture");
    g_material_diffuse_uniform = glGetUniformLocation(g_GpuProgramID, "materialDiffuse");
    g_material_specular_uniform = glGetUniformLocation(g_GpuProgramID, "materialSpecular");
    g_material_ambient_uniform = glGetUniformLocation(g_GpuProgramID, "materialAmbient");
    g_material_shininess_uniform = glGetUniformLocation(g_GpuProgramID, "materialShininess");

    // Variáveis em "shader_fragment.glsl" para acesso das imagens de textura
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage2"), 2);
    glUniform1i(g_player_texture_uniform, 3);
    glUniform3f(g_object_tint_uniform, 1.0f, 1.0f, 1.0f);
    glUseProgram(0);
}

// Função que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M)
{
    g_MatrixStack.push(M);
}

// Função que remove a matriz atualmente no topo da pilha e armazena a mesma na variável M
void PopMatrix(glm::mat4 &M)
{
    if (g_MatrixStack.empty())
    {
        M = Matrix_Identity();
    }
    else
    {
        M = g_MatrixStack.top();
        g_MatrixStack.pop();
    }
}

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel *model)
{
    if (!model->attrib.normals.empty())
        return;

    // Primeiro computamos as normais para todos os TRIÂNGULOS.
    // Segundo, computamos as normais dos VÉRTICES através do método proposto
    // por Gouraud, onde a normal de cada vértice vai ser a média das normais de
    // todas as faces que compartilham este vértice e que pertencem ao mesmo "smoothing group".

    // Obtemos a lista dos smoothing groups que existem no objeto
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        assert(model->shapes[shape].mesh.smoothing_group_ids.size() == num_triangles);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);
            unsigned int sgroup = model->shapes[shape].mesh.smoothing_group_ids[triangle];
            assert(sgroup >= 0);
            sgroup_ids.insert(sgroup);
        }
    }

    size_t num_vertices = model->attrib.vertices.size() / 3;
    model->attrib.normals.reserve(3 * num_vertices);

    // Processamos um smoothing group por vez
    for (const unsigned int &sgroup : sgroup_ids)
    {
        std::vector<int> num_triangles_per_vertex(num_vertices, 0);
        std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

        // Acumulamos as normais dos vértices de todos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                glm::vec4 vertices[3];
                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];
                    const float vx = model->attrib.vertices[3 * idx.vertex_index + 0];
                    const float vy = model->attrib.vertices[3 * idx.vertex_index + 1];
                    const float vz = model->attrib.vertices[3 * idx.vertex_index + 2];
                    vertices[vertex] = glm::vec4(vx, vy, vz, 1.0);
                }

                const glm::vec4 a = vertices[0];
                const glm::vec4 b = vertices[1];
                const glm::vec4 c = vertices[2];

                const glm::vec4 n = crossproduct(b - a, c - a);

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];
                    num_triangles_per_vertex[idx.vertex_index] += 1;
                    vertex_normals[idx.vertex_index] += n;
                }
            }
        }

        // Computamos a média das normais acumuladas
        std::vector<size_t> normal_indices(num_vertices, 0);

        for (size_t vertex_index = 0; vertex_index < vertex_normals.size(); ++vertex_index)
        {
            if (num_triangles_per_vertex[vertex_index] == 0)
                continue;

            glm::vec4 n = vertex_normals[vertex_index] / (float)num_triangles_per_vertex[vertex_index];
            n /= norm(n);

            model->attrib.normals.push_back(n.x);
            model->attrib.normals.push_back(n.y);
            model->attrib.normals.push_back(n.z);

            size_t normal_index = (model->attrib.normals.size() / 3) - 1;
            normal_indices[vertex_index] = normal_index;
        }

        // Escrevemos os índices das normais para os vértices dos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];
                    model->shapes[shape].mesh.indices[3 * triangle + vertex].normal_index =
                        normal_indices[idx.vertex_index];
                }
            }
        }
    }
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel *model, GLuint default_texture_id)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float> model_coefficients;
    std::vector<float> normal_coefficients;
    std::vector<float> texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_faces = model->shapes[shape].mesh.num_face_vertices.size();
        if (num_faces == 0) continue;

        const float minval = std::numeric_limits<float>::min();
        const float maxval = std::numeric_limits<float>::max();

        int current_material = -99;
        size_t face_group_start = 0;
        size_t group_index = 0;

        for (size_t face = 0; face <= num_faces; ++face)
        {
            int mat_id = (face < num_faces) ? model->shapes[shape].mesh.material_ids[face] : -99;

            if (face == 0)
            {
                current_material = mat_id;
                face_group_start = 0;
            }

            if (mat_id != current_material || face == num_faces)
            {
                // Cria sub-objeto para as faces [face_group_start, face-1]
                size_t group_first_index = indices.size();
                glm::vec4 bbox_min = glm::vec4(maxval, maxval, maxval, 1.0f);
                glm::vec4 bbox_max = glm::vec4(minval, minval, minval, 1.0f);

                for (size_t f = face_group_start; f < face; ++f)
                {
                    assert(model->shapes[shape].mesh.num_face_vertices[f] == 3);

                    for (size_t vertex = 0; vertex < 3; ++vertex)
                    {
                        tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * f + vertex];

                        indices.push_back(group_first_index + 3 * (f - face_group_start) + vertex);

                        const float vx = model->attrib.vertices[3 * idx.vertex_index + 0];
                        const float vy = model->attrib.vertices[3 * idx.vertex_index + 1];
                        const float vz = model->attrib.vertices[3 * idx.vertex_index + 2];
                        model_coefficients.push_back(vx);
                        model_coefficients.push_back(vy);
                        model_coefficients.push_back(vz);
                        model_coefficients.push_back(1.0f);

                        bbox_min.x = std::min(bbox_min.x, vx);
                        bbox_min.y = std::min(bbox_min.y, vy);
                        bbox_min.z = std::min(bbox_min.z, vz);
                        bbox_max.x = std::max(bbox_max.x, vx);
                        bbox_max.y = std::max(bbox_max.y, vy);
                        bbox_max.z = std::max(bbox_max.z, vz);

                        if (idx.normal_index != -1)
                        {
                            const float nx = model->attrib.normals[3 * idx.normal_index + 0];
                            const float ny = model->attrib.normals[3 * idx.normal_index + 1];
                            const float nz = model->attrib.normals[3 * idx.normal_index + 2];
                            normal_coefficients.push_back(nx);
                            normal_coefficients.push_back(ny);
                            normal_coefficients.push_back(nz);
                            normal_coefficients.push_back(0.0f);
                        }

                        if (idx.texcoord_index != -1)
                        {
                            const float u = model->attrib.texcoords[2 * idx.texcoord_index + 0];
                            const float v = model->attrib.texcoords[2 * idx.texcoord_index + 1];
                            texture_coefficients.push_back(u);
                            texture_coefficients.push_back(v);
                        }
                    }
                }

                size_t group_last_index = indices.size() - 1;

                SceneObject theobject;
                char namebuf[512];
                snprintf(namebuf, sizeof(namebuf), "%s_grp%zu", model->shapes[shape].name.c_str(), group_index);
                theobject.name = namebuf;
                theobject.first_index = group_first_index;
                theobject.num_indices = group_last_index - group_first_index + 1;
                theobject.rendering_mode = GL_TRIANGLES;
                theobject.vertex_array_object_id = vertex_array_object_id;

                if (current_material >= 0)
                {
                    theobject.texture_id = model->material_texture_ids[current_material];
                    if (theobject.texture_id == 0)
                        theobject.texture_id = default_texture_id;
                    theobject.sampler_id = 0;
                    for (auto const &[name, id] : g_TextureCache)
                    {
                        if (id == theobject.texture_id)
                        {
                            theobject.sampler_id = g_SamplerCache[name];
                            break;
                        }
                    }
                }
                else
                {
                    theobject.texture_id = default_texture_id;
                    theobject.sampler_id = 0;
                }

                theobject.bbox_min = bbox_min;
                theobject.bbox_max = bbox_max;

                g_VirtualScene[theobject.name] = theobject;

                current_material = mat_id;
                face_group_start = face;
                group_index++;
            }
        }
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0;            // "(location = 0)" em "shader_vertex.glsl"
    GLint number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLuint VBO_normal_coefficients_id;
    glGenBuffers(1, &VBO_normal_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
    location = 1;             // "(location = 1)" em "shader_vertex.glsl"
    number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLuint VBO_texture_coefficients_id;
    glGenBuffers(1, &VBO_texture_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
    location = 2;             // "(location = 1)" em "shader_vertex.glsl"
    number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLuint indices_id;
    glGenBuffers(1, &indices_id);

    // "Ligamos" o buffer. Note que o tipo agora é GL_ELEMENT_ARRAY_BUFFER.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!
    //

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

void BuildTrianglesFromAssimpAndAddToVirtualScene(AssimpModelLoader &loader)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float> model_coefficients;
    std::vector<float> normal_coefficients;
    std::vector<float> texture_coefficients;
    std::vector<unsigned int> bone_id_data;
    std::vector<float> bone_weight_data;

    const auto &meshes = loader.GetMeshes();
    
    printf("BuildTrianglesFromAssimp: %zu meshes\n", meshes.size());
    
    for (size_t mesh_idx = 0; mesh_idx < meshes.size(); ++mesh_idx)
    {
        const auto &mesh = meshes[mesh_idx];
        size_t first_index = indices.size();
        
        printf("  Mesh %zu: name='%s', vertices=%zu, indices=%zu, hasBoneData=%d\n", 
               mesh_idx, mesh.name.c_str(), mesh.vertices.size()/3, mesh.indices.size(),
               !mesh.boneData.empty());
        
        const float minval = std::numeric_limits<float>::min();
        const float maxval = std::numeric_limits<float>::max();
        glm::vec4 bbox_min = glm::vec4(maxval, maxval, maxval, 1.0f);
        glm::vec4 bbox_max = glm::vec4(minval, minval, minval, 1.0f);
        
        for (size_t i = 0; i < mesh.indices.size(); ++i)
        {
            unsigned int idx = mesh.indices[i];
            indices.push_back(first_index + i);
            
            const float vx = mesh.vertices[3 * idx + 0];
            const float vy = mesh.vertices[3 * idx + 1];
            const float vz = mesh.vertices[3 * idx + 2];
            model_coefficients.push_back(vx);
            model_coefficients.push_back(vy);
            model_coefficients.push_back(vz);
            model_coefficients.push_back(1.0f);
            
            bbox_min.x = std::min(bbox_min.x, vx);
            bbox_min.y = std::min(bbox_min.y, vy);
            bbox_min.z = std::min(bbox_min.z, vz);
            bbox_max.x = std::max(bbox_max.x, vx);
            bbox_max.y = std::max(bbox_max.y, vy);
            bbox_max.z = std::max(bbox_max.z, vz);
            
            if (!mesh.normals.empty())
            {
                const float nx = mesh.normals[3 * idx + 0];
                const float ny = mesh.normals[3 * idx + 1];
                const float nz = mesh.normals[3 * idx + 2];
                normal_coefficients.push_back(nx);
                normal_coefficients.push_back(ny);
                normal_coefficients.push_back(nz);
                normal_coefficients.push_back(0.0f);
            }
            
            if (!mesh.texcoords.empty())
            {
                const float u = mesh.texcoords[2 * idx + 0];
                const float v = mesh.texcoords[2 * idx + 1];
                texture_coefficients.push_back(u);
                texture_coefficients.push_back(v);
            }
            
            if (!mesh.boneData.empty())
            {
                const auto &bd = mesh.boneData[idx];
                bone_id_data.push_back(bd.boneIDs[0]);
                bone_id_data.push_back(bd.boneIDs[1]);
                bone_id_data.push_back(bd.boneIDs[2]);
                bone_id_data.push_back(bd.boneIDs[3]);
                bone_weight_data.push_back(bd.weights[0]);
                bone_weight_data.push_back(bd.weights[1]);
                bone_weight_data.push_back(bd.weights[2]);
                bone_weight_data.push_back(bd.weights[3]);
            }
            else
            {
                bone_id_data.push_back(0);
                bone_id_data.push_back(0);
                bone_id_data.push_back(0);
                bone_id_data.push_back(0);
                bone_weight_data.push_back(0.0f);
                bone_weight_data.push_back(0.0f);
                bone_weight_data.push_back(0.0f);
                bone_weight_data.push_back(0.0f);
            }
        }
        
        size_t last_index = indices.size() - 1;
        
        SceneObject theobject;
        theobject.name = mesh.name;
        theobject.first_index = first_index;
        theobject.num_indices = last_index - first_index + 1;
        theobject.rendering_mode = GL_TRIANGLES;
        theobject.vertex_array_object_id = vertex_array_object_id;
        theobject.texture_id = 0;
        theobject.sampler_id = 0;
        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;
        
        g_VirtualScene[mesh.name] = theobject;
    }
    
    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0;
    GLint number_of_dimensions = 4;
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    if (!normal_coefficients.empty())
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 1;
        number_of_dimensions = 4;
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    if (!texture_coefficients.empty())
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2;
        number_of_dimensions = 2;
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    // Bone IDs (location = 3, ivec4)
    {
        GLuint VBO_bone_ids;
        glGenBuffers(1, &VBO_bone_ids);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_bone_ids);
        glBufferData(GL_ARRAY_BUFFER, bone_id_data.size() * sizeof(unsigned int), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, bone_id_data.size() * sizeof(unsigned int), bone_id_data.data());
        location = 3;
        glVertexAttribIPointer(location, 4, GL_UNSIGNED_INT, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    // Bone weights (location = 4, vec4)
    {
        GLuint VBO_bone_weights;
        glGenBuffers(1, &VBO_bone_weights);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_bone_weights);
        glBufferData(GL_ARRAY_BUFFER, bone_weight_data.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, bone_weight_data.size() * sizeof(float), bone_weight_data.data());
        location = 4;
        glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    GLuint indices_id;
    glGenBuffers(1, &indices_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    
    glBindVertexArray(0);
}

void BuildUnitCubeAndAddToVirtualScene(const char *object_name)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    const float vertices[] = {
        // Frente (+Z)
        -0.5f, -0.5f, +0.5f, +0.5f, -0.5f, +0.5f, +0.5f, +0.5f, +0.5f,
        -0.5f, -0.5f, +0.5f, +0.5f, +0.5f, +0.5f, -0.5f, +0.5f, +0.5f,
        // Trás (-Z)
        +0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, +0.5f, -0.5f,
        +0.5f, -0.5f, -0.5f, -0.5f, +0.5f, -0.5f, +0.5f, +0.5f, -0.5f,
        // Esquerda (-X)
        -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, +0.5f, -0.5f, +0.5f, +0.5f,
        -0.5f, -0.5f, -0.5f, -0.5f, +0.5f, +0.5f, -0.5f, +0.5f, -0.5f,
        // Direita (+X)
        +0.5f, -0.5f, +0.5f, +0.5f, -0.5f, -0.5f, +0.5f, +0.5f, -0.5f,
        +0.5f, -0.5f, +0.5f, +0.5f, +0.5f, -0.5f, +0.5f, +0.5f, +0.5f,
        // Topo (+Y)
        -0.5f, +0.5f, +0.5f, +0.5f, +0.5f, +0.5f, +0.5f, +0.5f, -0.5f,
        -0.5f, +0.5f, +0.5f, +0.5f, +0.5f, -0.5f, -0.5f, +0.5f, -0.5f,
        // Base (-Y)
        -0.5f, -0.5f, -0.5f, +0.5f, -0.5f, -0.5f, +0.5f, -0.5f, +0.5f,
        -0.5f, -0.5f, -0.5f, +0.5f, -0.5f, +0.5f, -0.5f, -0.5f, +0.5f};

    const float normals[] = {
        0, 0, +1, 0, 0, +1, 0, 0, +1, 0, 0, +1, 0, 0, +1, 0, 0, +1,
        0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1,
        -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0,
        +1, 0, 0, +1, 0, 0, +1, 0, 0, +1, 0, 0, +1, 0, 0, +1, 0, 0,
        0, +1, 0, 0, +1, 0, 0, +1, 0, 0, +1, 0, 0, +1, 0, 0, +1, 0,
        0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0};

    std::vector<float> model_coefficients;
    std::vector<float> normal_coefficients;
    std::vector<float> texture_coefficients;
    std::vector<GLuint> indices;

    model_coefficients.reserve(36 * 4);
    normal_coefficients.reserve(36 * 4);
    texture_coefficients.reserve(36 * 2);
    indices.reserve(36);

    for (GLuint i = 0; i < 36; ++i)
    {
        model_coefficients.push_back(vertices[3 * i + 0]);
        model_coefficients.push_back(vertices[3 * i + 1]);
        model_coefficients.push_back(vertices[3 * i + 2]);
        model_coefficients.push_back(1.0f);

        normal_coefficients.push_back(normals[3 * i + 0]);
        normal_coefficients.push_back(normals[3 * i + 1]);
        normal_coefficients.push_back(normals[3 * i + 2]);
        normal_coefficients.push_back(0.0f);

        texture_coefficients.push_back(0.0f);
        texture_coefficients.push_back(0.0f);

        indices.push_back(i);
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), model_coefficients.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    GLuint VBO_normal_coefficients_id;
    glGenBuffers(1, &VBO_normal_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), normal_coefficients.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);

    GLuint VBO_texture_coefficients_id;
    glGenBuffers(1, &VBO_texture_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), texture_coefficients.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(2);

    GLuint indices_id;
    glGenBuffers(1, &indices_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    SceneObject cube;
    cube.name = object_name;
    cube.first_index = 0;
    cube.num_indices = indices.size();
    cube.rendering_mode = GL_TRIANGLES;
    cube.vertex_array_object_id = vertex_array_object_id;
    cube.bbox_min = glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f);
    cube.bbox_max = glm::vec4(+0.5f, +0.5f, +0.5f, 1.0f);
    g_VirtualScene[object_name] = cube;

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void BuildCollisionDataFromObjModel(ObjModel *model, glm::mat4 model_matrix)
{
    g_ScenarioCollisionShapes.clear();
    g_ScenarioBoundsMin = glm::vec4(+std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), 1.0f);
    g_ScenarioBoundsMax = glm::vec4(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        CollisionShape shape_collision;
        shape_collision.bbox_min = glm::vec4(+std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), 1.0f);
        shape_collision.bbox_max = glm::vec4(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);

        const size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
        shape_collision.triangles.reserve(num_triangles);

        if (model->shapes[shape].name.find("DOOR") != std::string::npos)
        {
            shape_collision.type = CollisionShapeType::DOOR;
        }
        else if (model->shapes[shape].name.find("VINES") != std::string::npos)
        {
            shape_collision.type = CollisionShapeType::VINES;
        }
        else if (model->shapes[shape].name.find("LADDER") != std::string::npos)
        {
            shape_collision.type = CollisionShapeType::LADDER;
        }
        else if (model->shapes[shape].name.find("WATER") != std::string::npos)
        {
            shape_collision.type = CollisionShapeType::WATER;
        }
        else
        {
            shape_collision.type = CollisionShapeType::SOLID;
        }

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            Triangle world_triangle;
            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                const tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];
                const glm::vec4 p_model(
                    model->attrib.vertices[3 * idx.vertex_index + 0],
                    model->attrib.vertices[3 * idx.vertex_index + 1],
                    model->attrib.vertices[3 * idx.vertex_index + 2],
                    1.0f);
                const glm::vec4 p_world = model_matrix * p_model;
                const glm::vec4 p = glm::vec4(p_world.x, p_world.y, p_world.z, 1.0f);

                if (vertex == 0)
                    world_triangle.v1 = p;
                if (vertex == 1)
                    world_triangle.v2 = p;
                if (vertex == 2)
                    world_triangle.v3 = p;

                shape_collision.bbox_min.x = std::min(shape_collision.bbox_min.x, p.x);
                shape_collision.bbox_min.y = std::min(shape_collision.bbox_min.y, p.y);
                shape_collision.bbox_min.z = std::min(shape_collision.bbox_min.z, p.z);
                shape_collision.bbox_max.x = std::max(shape_collision.bbox_max.x, p.x);
                shape_collision.bbox_max.y = std::max(shape_collision.bbox_max.y, p.y);
                shape_collision.bbox_max.z = std::max(shape_collision.bbox_max.z, p.z);
            }

            shape_collision.triangles.push_back(world_triangle);
        }

        g_ScenarioBoundsMin.x = std::min(g_ScenarioBoundsMin.x, shape_collision.bbox_min.x);
        g_ScenarioBoundsMin.y = std::min(g_ScenarioBoundsMin.y, shape_collision.bbox_min.y);
        g_ScenarioBoundsMin.z = std::min(g_ScenarioBoundsMin.z, shape_collision.bbox_min.z);
        g_ScenarioBoundsMax.x = std::max(g_ScenarioBoundsMax.x, shape_collision.bbox_max.x);
        g_ScenarioBoundsMax.y = std::max(g_ScenarioBoundsMax.y, shape_collision.bbox_max.y);
        g_ScenarioBoundsMax.z = std::max(g_ScenarioBoundsMax.z, shape_collision.bbox_max.z);

        g_ScenarioCollisionShapes.push_back(shape_collision);
    }
}

void BuildCollisionDataIntoVector(ObjModel *model, glm::mat4 model_matrix,
    std::vector<CollisionShape> &out_shapes,
    glm::vec4 &out_bbox_min, glm::vec4 &out_bbox_max)
{
    out_bbox_min = glm::vec4(+std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), 1.0f);
    out_bbox_max = glm::vec4(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        CollisionShape shape_collision;
        shape_collision.bbox_min = glm::vec4(+std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), +std::numeric_limits<float>::infinity(), 1.0f);
        shape_collision.bbox_max = glm::vec4(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);

        const size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
        shape_collision.triangles.reserve(num_triangles);

        if (model->shapes[shape].name.find("DOOR") != std::string::npos)
        {
            shape_collision.type = CollisionShapeType::DOOR;
        }
        else if (model->shapes[shape].name.find("VINES") != std::string::npos)
        {
            shape_collision.type = CollisionShapeType::VINES;
        }
        else if (model->shapes[shape].name.find("LADDER") != std::string::npos)
        {
            shape_collision.type = CollisionShapeType::LADDER;
        }
        else if (model->shapes[shape].name.find("WATER") != std::string::npos)
        {
            shape_collision.type = CollisionShapeType::WATER;
        }
        else
        {
            shape_collision.type = CollisionShapeType::SOLID;
        }

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            Triangle world_triangle;
            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                const tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];
                const glm::vec4 p_model(
                    model->attrib.vertices[3 * idx.vertex_index + 0],
                    model->attrib.vertices[3 * idx.vertex_index + 1],
                    model->attrib.vertices[3 * idx.vertex_index + 2],
                    1.0f);
                const glm::vec4 p_world = model_matrix * p_model;
                const glm::vec4 p = glm::vec4(p_world.x, p_world.y, p_world.z, 1.0f);

                if (vertex == 0)
                    world_triangle.v1 = p;
                if (vertex == 1)
                    world_triangle.v2 = p;
                if (vertex == 2)
                    world_triangle.v3 = p;

                shape_collision.bbox_min.x = std::min(shape_collision.bbox_min.x, p.x);
                shape_collision.bbox_min.y = std::min(shape_collision.bbox_min.y, p.y);
                shape_collision.bbox_min.z = std::min(shape_collision.bbox_min.z, p.z);
                shape_collision.bbox_max.x = std::max(shape_collision.bbox_max.x, p.x);
                shape_collision.bbox_max.y = std::max(shape_collision.bbox_max.y, p.y);
                shape_collision.bbox_max.z = std::max(shape_collision.bbox_max.z, p.z);
            }

            shape_collision.triangles.push_back(world_triangle);
        }

        out_bbox_min.x = std::min(out_bbox_min.x, shape_collision.bbox_min.x);
        out_bbox_min.y = std::min(out_bbox_min.y, shape_collision.bbox_min.y);
        out_bbox_min.z = std::min(out_bbox_min.z, shape_collision.bbox_min.z);
        out_bbox_max.x = std::max(out_bbox_max.x, shape_collision.bbox_max.x);
        out_bbox_max.y = std::max(out_bbox_max.y, shape_collision.bbox_max.y);
        out_bbox_max.z = std::max(out_bbox_max.z, shape_collision.bbox_max.z);

        out_shapes.push_back(shape_collision);
    }
}

bool FindCameraObstructionDistance(const glm::vec4 &pivot_position, const glm::vec4 &desired_camera_position, float &hit_distance)
{
    const glm::vec4 camera_path = desired_camera_position - pivot_position;
    const float max_distance = norm(camera_path);
    hit_distance = max_distance;

    if (max_distance < 0.0001f)
        return false;

    const glm::vec4 direction = camera_path / max_distance;
    const float sample_spacing = std::max(0.04f, g_CameraCollisionHalfExtents.x * 0.5f);
    const int sample_count = std::max(1, static_cast<int>(std::ceil(max_distance / sample_spacing)));

    float last_valid_distance = 0.0f;
    for (int sample_index = 1; sample_index <= sample_count; ++sample_index)
    {
        const float distance = std::min(max_distance, (max_distance * sample_index) / static_cast<float>(sample_count));
        const glm::vec4 sample_center = pivot_position + direction * distance;
        const CollisionShapeType collision =
            CollidesWithScenarioAabb(sample_center, g_CameraCollisionHalfExtents, g_ScenarioCollisionShapes);

        if (collision == CollisionShapeType::SOLID || collision == CollisionShapeType::DOOR)
        {
            hit_distance = last_valid_distance;
            return true;
        }

        last_valid_distance = distance;
    }

    hit_distance = max_distance;
    return false;
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char *filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos vértices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definição de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char *filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU de
// um arquivo GLSL e faz sua compilação.
void LoadShader(const char *filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela variável "filename"
    // e colocamos seu conteúdo em memória, apontado pela variável
    // "shader_string".
    std::ifstream file;
    try
    {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    }
    catch (std::exception &e)
    {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar *shader_string = str.c_str();
    const GLint shader_string_length = static_cast<GLint>(str.length());

    // Define o código do shader GLSL, contido na string "shader_string"
    glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

    // Compila o código do shader GLSL (em tempo de execução)
    glCompileShader(shader_id);

    // Verificamos se ocorreu algum erro ou "warning" durante a compilação
    GLint compiled_ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memória para guardar o log de compilação.
    // A chamada "new" em C++ é equivalente ao "malloc()" do C.
    GLchar *log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilação
    if (log_length != 0)
    {
        std::string output;

        if (!compiled_ok)
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
            output += "WARNING: OpenGL compilation of \"";
            output += filename;
            output += "\".\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }

        fprintf(stderr, "%s", output.c_str());
    }

    // A chamada "delete" em C++ é equivalente ao "free()" do C
    delete[] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    // Criamos um identificador (ID) para este programa de GPU
    GLuint program_id = glCreateProgram();

    // Definição dos dois shaders GLSL que devem ser executados pelo programa
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    // Linkagem dos shaders acima ao programa
    glLinkProgram(program_id);

    // Verificamos se ocorreu algum erro durante a linkagem
    GLint linked_ok = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

    // Imprime no terminal qualquer erro de linkagem
    if (linked_ok == GL_FALSE)
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memória para guardar o log de compilação.
        // A chamada "new" em C++ é equivalente ao "malloc()" do C.
        GLchar *log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ é equivalente ao "free()" do C
        delete[] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para deleção após serem linkados
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    // Retornamos o ID gerado acima
    return program_id;
}

// Definição da função que será chamada sempre que a janela do sistema
// operacional for redimensionada, por consequência alterando o tamanho do
// "framebuffer" (região de memória onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    // Indicamos que queremos renderizar em toda região do framebuffer. A
    // função "glViewport" define o mapeamento das "normalized device
    // coordinates" (NDC) para "pixel coordinates".  Essa é a operação de
    // "Screen Mapping" ou "Viewport Mapping" vista em aula ({+ViewportMapping2+}).
    glViewport(0, 0, width, height);

    // Atualizamos também a razão que define a proporção da janela (largura /
    // altura), a qual será utilizada na definição das matrizes de projeção,
    // tal que não ocorra distorções durante o processo de "Screen Mapping"
    // acima, quando NDC é mapeado para coordenadas de pixels. Veja slides 205-215 do documento Aula_09_Projecoes.pdf.
    //
    // O cast para float é necessário pois números inteiros são arredondados ao
    // serem divididos!
    g_ScreenRatio = (float)width / height;
}

// Variáveis globais que armazenam a última posição do cursor do mouse, para
// que possamos calcular quanto que o mouse se movimentou entre dois instantes
// de tempo. Utilizadas no callback CursorPosCallback() abaixo.
double g_LastCursorPosX, g_LastCursorPosY;

// Função callback chamada sempre que o usuário aperta algum dos botões do mouse
void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_LeftMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_LeftMouseButtonPressed = true;

        // Inicializamos Theta e Phi a partir do camera_view_vector atual para evitar pulos
        g_CameraPhi = std::asin(camera_view_vector.y / glm::length(camera_view_vector));
        g_CameraTheta = std::atan2(-camera_view_vector.x, camera_view_vector.z);

        if (g_FirstPersonCamera)
        {
            BeginSlingshotCharge();
        }
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_LeftMouseButtonPressed = false;

        if (g_FirstPersonCamera && g_SlingshotState.is_charging)
        {
            QueueSlingshotShot();
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_RightMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_RightMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_RightMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_MiddleMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_MiddleMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.mouse
        g_MiddleMouseButtonPressed = false;
    }
}

// Função callback chamada sempre que o usuário movimentar o cursor do mouse em
// cima da janela OpenGL.
void CursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
    float dx = xpos - g_LastCursorPosX;
    float dy = ypos - g_LastCursorPosY;

    g_LastCursorPosX = xpos;
    g_LastCursorPosY = ypos;

    if (g_FirstPersonCamera)
    {
        g_CameraTheta += 0.01f * dx;
        g_CameraPhi -= 0.01f * dy;

        // limite de diferença câmera/personagem
        float max_angle = glm::radians(0.01f);

        float angle_diff =
            WrapAnglePi(g_CameraTheta - g_PlayerYaw);

        if (angle_diff > max_angle)
        {
            g_PlayerYaw = g_CameraTheta - max_angle;
        }
        else if (angle_diff < -max_angle)
        {
            g_PlayerYaw = g_CameraTheta + max_angle;
        }

        const float phimax = 1.45f;
        const float phimin = -1.45f;

        if (g_CameraPhi > phimax)
            g_CameraPhi = phimax;
        if (g_CameraPhi < phimin)
            g_CameraPhi = phimin;

        float vx = -std::sin(g_CameraTheta) * std::cos(g_CameraPhi);
        float vy = std::sin(g_CameraPhi);
        float vz = std::cos(g_CameraTheta) * std::cos(g_CameraPhi);

        camera_view_vector = glm::vec4(vx, vy, vz, 0.0f);
    }
}

// Função callback chamada sempre que o usuário movimenta a "rodinha" do mouse.
void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    // Atualizamos a distância da câmera para a origem utilizando a
    // movimentação da "rodinha", simulando um ZOOM.
    g_CameraDistance -= 0.1f * yoffset;

    // Uma câmera look-at nunca pode estar exatamente "em cima" do ponto para
    // onde ela está olhando, pois isto gera problemas de divisão por zero na
    // definição do sistema de coordenadas da câmera. Isto é, a variável abaixo
    // nunca pode ser zero. Versões anteriores deste código possuíam este bug,
    // o qual foi detectado pelo aluno Vinicius Fraga (2017/2).
    const float verysmallnumber = std::numeric_limits<float>::epsilon();
    if (g_CameraDistance < verysmallnumber)
        g_CameraDistance = verysmallnumber;
}

void Correcao_KeyCallback(int key, int action, int mod);

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mod)
{
    // =======================
    // Não modifique esta chamada! Ela é utilizada para correção automatizada dos
    // laboratórios. Deve ser sempre o primeiro comando desta função KeyCallback().
    Correcao_KeyCallback(key, action, mod);
    // =======================

    // Se o usuário pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // Se o usuário apertar a tecla P, mudará o tipo de câmera entre primeira pessoa e terceira pessoa. Veja definição das variáveis globais g_FirstPersonCamera e g_ThirdPersonCamera no início deste arquivo.
    if (key == GLFW_KEY_P && action == GLFW_PRESS && !g_IsClimbingAVine && !g_IsClimbingALadder)
    {
        g_FirstPersonCamera = !g_FirstPersonCamera;
        g_ThirdPersonCamera = !g_ThirdPersonCamera;
        g_CameraInitialized = false;
        g_CurrentThirdPersonCameraDistance = g_ThirdPersonCameraDistance;

        if (g_FirstPersonCamera)
        {
            g_WPressed = false;
            g_APressed = false;
            g_SPressed = false;
            g_DPressed = false;
            g_CameraTheta = g_PlayerYaw;
            g_CameraPhi = 0.0f;
            camera_view_vector = glm::vec4(-std::sin(g_PlayerYaw), 0.0f, std::cos(g_PlayerYaw), 0.0f);
        }
        else
        {
            g_SlingshotState.is_charging = false;
            g_SlingshotState.fire_requested = false;
            g_SlingshotState.charge_time_seconds = 0.0f;
            g_SlingshotState.queued_charge_ratio = 0.0f;
            g_SlingshotState.shot_cooldown_timer = 0.0f;
        }
    }

    // Se o usuário apertar a tecla R, recarregamos os shaders dos arquivos "shader_fragment.glsl" e "shader_vertex.glsl".
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        LoadShadersFromFiles();
        fprintf(stdout, "Shaders recarregados!\n");
        fflush(stdout);
    }

    // F1 toggles debug: coords + hitboxes
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
    {
        g_ShowPlayerCoords = !g_ShowPlayerCoords;
        g_ShowDebugHitboxes = !g_ShowDebugHitboxes;
        fprintf(stdout, "Debug: coords=%s hitboxes=%s\n",
                g_ShowPlayerCoords ? "ON" : "OFF",
                g_ShowDebugHitboxes ? "ON" : "OFF");
        fflush(stdout);
    }

    if (key == GLFW_KEY_O && action == GLFW_PRESS && g_CollidedWithAVine)
    {
        g_IsClimbingAVine = !g_IsClimbingAVine;
        if (g_IsClimbingAVine)
            printf("Started climbing vine!\n");
        else
            printf("Stopped climbing vine!\n");
    }
    if (key == GLFW_KEY_O && action == GLFW_PRESS && g_CollidedWithALadder)
    {
        g_IsClimbingALadder = !g_IsClimbingALadder;
        if (g_IsClimbingALadder)
            printf("Started climbing ladder!\n");
        else
            printf("Stopped climbing ladder!\n");
    }

    // Atualiza flags de input para movimento (desabilitado em primeira pessoa)
    if (key == GLFW_KEY_W)
        g_WPressed = (action != GLFW_RELEASE) && !g_FirstPersonCamera;
    if (key == GLFW_KEY_A)
        g_APressed = (action != GLFW_RELEASE) && !g_FirstPersonCamera;
    if (key == GLFW_KEY_S)
        g_SPressed = (action != GLFW_RELEASE) && !g_FirstPersonCamera;
    if (key == GLFW_KEY_D)
        g_DPressed = (action != GLFW_RELEASE) && !g_FirstPersonCamera;
    if (key == GLFW_KEY_SPACE)
        g_SpacePressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
        g_ShiftPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_E)
        g_AttackPressed = (action == GLFW_PRESS);
    if (key == GLFW_KEY_Q)
        g_DefendPressed = (action != GLFW_RELEASE);
}
// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char *description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

// Esta função recebe um vértice com coordenadas de modelo p_model e passa o
// mesmo por todos os sistemas de coordenadas armazenados nas matrizes model,
// view, e projection; e escreve na tela as matrizes e pontos resultantes
// dessas transformações.
void TextRendering_ShowModelViewProjection(
    GLFWwindow *window,
    glm::mat4 projection,
    glm::mat4 view,
    glm::mat4 model,
    glm::vec4 p_model)
{
    if (!g_ShowInfoText)
        return;

    glm::vec4 p_world = model * p_model;
    glm::vec4 p_camera = view * p_world;
    glm::vec4 p_clip = projection * p_camera;
    glm::vec4 p_ndc = p_clip / p_clip.w;

    float pad = TextRendering_LineHeight(window);

    TextRendering_PrintString(window, " Model matrix             Model     In World Coords.", -1.0f, 1.0f - pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, model, p_model, -1.0f, 1.0f - 2 * pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f - 6 * pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f - 7 * pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f - 8 * pad, 1.0f);

    TextRendering_PrintString(window, " View matrix              World     In Camera Coords.", -1.0f, 1.0f - 9 * pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, view, p_world, -1.0f, 1.0f - 10 * pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f - 14 * pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f - 15 * pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f - 16 * pad, 1.0f);

    TextRendering_PrintString(window, " Projection matrix        Camera                    In NDC", -1.0f, 1.0f - 17 * pad, 1.0f);
    TextRendering_PrintMatrixVectorProductDivW(window, projection, p_camera, -1.0f, 1.0f - 18 * pad, 1.0f);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glm::vec2 a = glm::vec2(-1, -1);
    glm::vec2 b = glm::vec2(+1, +1);
    glm::vec2 p = glm::vec2(0, 0);
    glm::vec2 q = glm::vec2(width, height);

    glm::mat4 viewport_mapping = Matrix(
        (q.x - p.x) / (b.x - a.x), 0.0f, 0.0f, (b.x * p.x - a.x * q.x) / (b.x - a.x),
        0.0f, (q.y - p.y) / (b.y - a.y), 0.0f, (b.y * p.y - a.y * q.y) / (b.y - a.y),
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    TextRendering_PrintString(window, "                                                       |  ", -1.0f, 1.0f - 22 * pad, 1.0f);
    TextRendering_PrintString(window, "                            .--------------------------'  ", -1.0f, 1.0f - 23 * pad, 1.0f);
    TextRendering_PrintString(window, "                            V                           ", -1.0f, 1.0f - 24 * pad, 1.0f);

    TextRendering_PrintString(window, " Viewport matrix           NDC      In Pixel Coords.", -1.0f, 1.0f - 25 * pad, 1.0f);
    TextRendering_PrintMatrixVectorProductMoreDigits(window, viewport_mapping, p_ndc, -1.0f, 1.0f - 26 * pad, 1.0f);
}

// Escrevemos na tela os ângulos de Euler definidos nas variáveis globais
// g_AngleX, g_AngleY, e g_AngleZ.
void TextRendering_ShowEulerAngles(GLFWwindow *window)
{
    if (!g_ShowInfoText)
        return;

    float pad = TextRendering_LineHeight(window);

    char buffer[80];
    snprintf(buffer, 80, "Euler Angles rotation matrix = Z(%.2f)*Y(%.2f)*X(%.2f)\n", g_AngleZ, g_AngleY, g_AngleX);

    TextRendering_PrintString(window, buffer, -1.0f + pad / 10, -1.0f + 2 * pad / 10, 1.0f);
}

// Escrevemos na tela qual matriz de projeção está sendo utilizada.
void TextRendering_ShowProjection(GLFWwindow *window)
{
    if (!g_ShowInfoText)
        return;

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    if (g_UsePerspectiveProjection)
        TextRendering_PrintString(window, "Perspective", 1.0f - 13 * charwidth, -1.0f + 2 * lineheight / 10, 1.0f);
    else
        TextRendering_PrintString(window, "Orthographic", 1.0f - 13 * charwidth, -1.0f + 2 * lineheight / 10, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow *window)
{
    if (!g_ShowInfoText)
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    static float old_seconds = (float)glfwGetTime();
    static int ellapsed_frames = 0;
    static char buffer[20] = "?? fps";
    static int numchars = 7;

    ellapsed_frames += 1;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    // Número de segundos desde o último cálculo do fps
    float ellapsed_seconds = seconds - old_seconds;

    if (ellapsed_seconds > 1.0f)
    {
        numchars = snprintf(buffer, 20, "%.2f fps", ellapsed_frames / ellapsed_seconds);

        old_seconds = seconds;
        ellapsed_frames = 0;
    }

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    TextRendering_PrintString(window, buffer, 1.0f - (numchars + 1) * charwidth, 1.0f - lineheight, 1.0f);
}

// Função para debugging: imprime no terminal todas informações de um modelo
// geométrico carregado de um arquivo ".obj".
// Veja: https://github.com/syoyo/tinyobjloader/blob/22883def8db9ef1f3ffb9b404318e7dd25fdbb51/loader_example.cc#L98
void PrintObjModelInfo(ObjModel *model)
{
    const tinyobj::attrib_t &attrib = model->attrib;
    const std::vector<tinyobj::shape_t> &shapes = model->shapes;
    const std::vector<tinyobj::material_t> &materials = model->materials;

    printf("# of vertices  : %d\n", (int)(attrib.vertices.size() / 3));
    printf("# of normals   : %d\n", (int)(attrib.normals.size() / 3));
    printf("# of texcoords : %d\n", (int)(attrib.texcoords.size() / 2));
    printf("# of shapes    : %d\n", (int)shapes.size());
    printf("# of materials : %d\n", (int)materials.size());

    for (size_t v = 0; v < attrib.vertices.size() / 3; v++)
    {
        printf("  v[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
               static_cast<const double>(attrib.vertices[3 * v + 0]),
               static_cast<const double>(attrib.vertices[3 * v + 1]),
               static_cast<const double>(attrib.vertices[3 * v + 2]));
    }

    for (size_t v = 0; v < attrib.normals.size() / 3; v++)
    {
        printf("  n[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
               static_cast<const double>(attrib.normals[3 * v + 0]),
               static_cast<const double>(attrib.normals[3 * v + 1]),
               static_cast<const double>(attrib.normals[3 * v + 2]));
    }

    for (size_t v = 0; v < attrib.texcoords.size() / 2; v++)
    {
        printf("  uv[%ld] = (%f, %f)\n", static_cast<long>(v),
               static_cast<const double>(attrib.texcoords[2 * v + 0]),
               static_cast<const double>(attrib.texcoords[2 * v + 1]));
    }

    // For each shape
    for (size_t i = 0; i < shapes.size(); i++)
    {
        printf("shape[%ld].name = %s\n", static_cast<long>(i),
               shapes[i].name.c_str());
        printf("Size of shape[%ld].indices: %lu\n", static_cast<long>(i),
               static_cast<unsigned long>(shapes[i].mesh.indices.size()));

        size_t index_offset = 0;

        assert(shapes[i].mesh.num_face_vertices.size() ==
               shapes[i].mesh.material_ids.size());

        printf("shape[%ld].num_faces: %lu\n", static_cast<long>(i),
               static_cast<unsigned long>(shapes[i].mesh.num_face_vertices.size()));

        // For each face
        for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++)
        {
            size_t fnum = shapes[i].mesh.num_face_vertices[f];

            printf("  face[%ld].fnum = %ld\n", static_cast<long>(f),
                   static_cast<unsigned long>(fnum));

            // For each vertex in the face
            for (size_t v = 0; v < fnum; v++)
            {
                tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
                printf("    face[%ld].v[%ld].idx = %d/%d/%d\n", static_cast<long>(f),
                       static_cast<long>(v), idx.vertex_index, idx.normal_index,
                       idx.texcoord_index);
            }

            printf("  face[%ld].material_id = %d\n", static_cast<long>(f),
                   shapes[i].mesh.material_ids[f]);

            index_offset += fnum;
        }

        printf("shape[%ld].num_tags: %lu\n", static_cast<long>(i),
               static_cast<unsigned long>(shapes[i].mesh.tags.size()));
        for (size_t t = 0; t < shapes[i].mesh.tags.size(); t++)
        {
            printf("  tag[%ld] = %s ", static_cast<long>(t),
                   shapes[i].mesh.tags[t].name.c_str());
            printf(" ints: [");
            for (size_t j = 0; j < shapes[i].mesh.tags[t].intValues.size(); ++j)
            {
                printf("%ld", static_cast<long>(shapes[i].mesh.tags[t].intValues[j]));
                if (j < (shapes[i].mesh.tags[t].intValues.size() - 1))
                {
                    printf(", ");
                }
            }
            printf("]");

            printf(" floats: [");
            for (size_t j = 0; j < shapes[i].mesh.tags[t].floatValues.size(); ++j)
            {
                printf("%f", static_cast<const double>(
                                 shapes[i].mesh.tags[t].floatValues[j]));
                if (j < (shapes[i].mesh.tags[t].floatValues.size() - 1))
                {
                    printf(", ");
                }
            }
            printf("]");

            printf(" strings: [");
            for (size_t j = 0; j < shapes[i].mesh.tags[t].stringValues.size(); ++j)
            {
                printf("%s", shapes[i].mesh.tags[t].stringValues[j].c_str());
                if (j < (shapes[i].mesh.tags[t].stringValues.size() - 1))
                {
                    printf(", ");
                }
            }
            printf("]");
            printf("\n");
        }
    }

    for (size_t i = 0; i < materials.size(); i++)
    {
        printf("material[%ld].name = %s\n", static_cast<long>(i),
               materials[i].name.c_str());
        printf("  material.Ka = (%f, %f ,%f)\n",
               static_cast<const double>(materials[i].ambient[0]),
               static_cast<const double>(materials[i].ambient[1]),
               static_cast<const double>(materials[i].ambient[2]));
        printf("  material.Kd = (%f, %f ,%f)\n",
               static_cast<const double>(materials[i].diffuse[0]),
               static_cast<const double>(materials[i].diffuse[1]),
               static_cast<const double>(materials[i].diffuse[2]));
        printf("  material.Ks = (%f, %f ,%f)\n",
               static_cast<const double>(materials[i].specular[0]),
               static_cast<const double>(materials[i].specular[1]),
               static_cast<const double>(materials[i].specular[2]));
        printf("  material.Tr = (%f, %f ,%f)\n",
               static_cast<const double>(materials[i].transmittance[0]),
               static_cast<const double>(materials[i].transmittance[1]),
               static_cast<const double>(materials[i].transmittance[2]));
        printf("  material.Ke = (%f, %f ,%f)\n",
               static_cast<const double>(materials[i].emission[0]),
               static_cast<const double>(materials[i].emission[1]),
               static_cast<const double>(materials[i].emission[2]));
        printf("  material.Ns = %f\n",
               static_cast<const double>(materials[i].shininess));
        printf("  material.Ni = %f\n", static_cast<const double>(materials[i].ior));
        printf("  material.dissolve = %f\n",
               static_cast<const double>(materials[i].dissolve));
        printf("  material.illum = %d\n", materials[i].illum);
        printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
        printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
        printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
        printf("  material.map_Ns = %s\n",
               materials[i].specular_highlight_texname.c_str());
        printf("  material.map_bump = %s\n", materials[i].bump_texname.c_str());
        printf("  material.map_d = %s\n", materials[i].alpha_texname.c_str());
        printf("  material.disp = %s\n", materials[i].displacement_texname.c_str());
        printf("  <<PBR>>\n");
        printf("  material.Pr     = %f\n", materials[i].roughness);
        printf("  material.Pm     = %f\n", materials[i].metallic);
        printf("  material.Ps     = %f\n", materials[i].sheen);
        printf("  material.Pc     = %f\n", materials[i].clearcoat_thickness);
        printf("  material.Pcr    = %f\n", materials[i].clearcoat_thickness);
        printf("  material.aniso  = %f\n", materials[i].anisotropy);
        printf("  material.anisor = %f\n", materials[i].anisotropy_rotation);
        printf("  material.map_Ke = %s\n", materials[i].emissive_texname.c_str());
        printf("  material.map_Pr = %s\n", materials[i].roughness_texname.c_str());
        printf("  material.map_Pm = %s\n", materials[i].metallic_texname.c_str());
        printf("  material.map_Ps = %s\n", materials[i].sheen_texname.c_str());
        printf("  material.norm   = %s\n", materials[i].normal_texname.c_str());
        std::map<std::string, std::string>::const_iterator it(
            materials[i].unknown_parameter.begin());
        std::map<std::string, std::string>::const_iterator itEnd(
            materials[i].unknown_parameter.end());

        for (; it != itEnd; it++)
        {
            printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
        }
        printf("\n");
    }
}

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :
